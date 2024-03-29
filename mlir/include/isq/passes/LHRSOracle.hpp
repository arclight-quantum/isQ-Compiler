#pragma once
#include <set>
#include "caterpillar/synthesis/lhrs.hpp"

using Qubit = tweedledum::qubit_id;
using SetQubits = std::vector<Qubit>;

namespace caterpillar {

namespace mt = mockturtle;

namespace detail {

template<class QuantumNetwork, class LogicNetwork, class SingleTargetGateSynthesisFn>
class logic_network_synthesis_impl_oracle
{
  using node_t = typename LogicNetwork::node;
public:
  logic_network_synthesis_impl_oracle( QuantumNetwork& qnet, LogicNetwork const& ntk,
                                mapping_strategy<LogicNetwork>& strategy,
                                SingleTargetGateSynthesisFn const& stg_fn,
                                logic_network_synthesis_params const& ps,
                                logic_network_synthesis_stats& st )
      : qnet( qnet ), ntk( ntk ), strategy( strategy ), stg_fn( stg_fn ), ps( ps ), st( st )
  {
  }
  bool run()
  {
    mockturtle::stopwatch t( st.time_total );
    prepare_inputs();
    prepare_constant( false );
    record_pipos();
    if ( ntk.get_node( ntk.get_constant( false ) ) != ntk.get_node( ntk.get_constant( true ) ) )
      prepare_constant( true );

    if ( const auto result = strategy.compute_steps( ntk ); !result )
    {
    
      std::cout << "[i] strategy could not be computed\n";
      return false;
    }
    strategy.foreach_step( [&]( auto node, auto action ) {
      std::visit(
          overloaded{
              []( auto ) {},
              [&]( compute_action const& action ) {
                const auto t = (pos.count(node) ? request_clean_ancilla() : request_ancilla());
                node_to_qubit[node].push(t);
                if ( ps.verbose )
                {
                  fmt::print("[i] compute {} in qubit {}\n", node, t);
                  if ( action.leaves )   
                    fmt::print(" with leaves: {} \n", fmt::join(*action.leaves, " ")); 
                }
                if ( action.cell_override )
                {
                  const auto [func, leaves] = *action.cell_override;
                  compute_node_as_cell( node, t, func, leaves );
                }
                else if (action.leaves)
                {
                  compute_big_xor( t, *action.leaves );
                }
                else
                {
                  compute_node( node, t ); 
                }
              },
              [&]( uncompute_action const& action ) {
                const auto t = node_to_qubit[node].top();
                if ( ps.verbose )
                {  
                  fmt::print("[i] uncompute {} from qubit {}\n", node, t);
                  if ( action.leaves )   
                    fmt::print(" with leaves: {} \n", fmt::join(*action.leaves, " ")); 
                }
                if ( action.cell_override )
                {
                  const auto [func, leaves] = *action.cell_override;
                  compute_node_as_cell( node, t, func, leaves );
                }
                else if (action.leaves)
                {
                  compute_big_xor(  t, *action.leaves );
                }
                else
                {
                  compute_node( node, t );
                }
                release_ancilla( t );
              },
              [&]( compute_inplace_action const& action ) {
                const auto t = node_to_qubit[ntk.index_to_node( action.target_index )].top() ;
                node_to_qubit[node].push(t);

                if ( ps.verbose )
                {  
                  fmt::print("[i] compute {} inplace onto node {}\n", node , action.target_index);
                  if ( action.leaves )   
                    fmt::print(" with leaves: {} \n", fmt::join(*action.leaves, " ")); 
                }
                if (action.leaves)
                {
                  compute_big_xor(t, *action.leaves);
                }
                else
                {  
                  compute_node_inplace( node, t );
                }
              },
              [&]( uncompute_inplace_action const& action ) {
                const auto t = node_to_qubit[node].top();
                if ( ps.verbose )
                {
                  fmt::print("[i] uncompute {} inplace to {}\n", node , action.target_index);
                  if ( action.leaves )   
                    fmt::print(" with leaves: {} \n", fmt::join(*action.leaves, " ")); 
                }
                if (action.leaves)
                {
                  compute_big_xor(  t, *action.leaves );
                }
                else 
                {  
                  compute_node_inplace( node, t );
                }
              },
              [&] (buffer_action const& action){
                if(ps.verbose)
                {
                  fmt::print("[i] compute buffer from node {} to node {}\n", action.leaf, action.target);
                }
                node_to_qubit[action.target].push( node_to_qubit[action.leaf].top() );
              },
              [&] (compute_level_action const& action){
                if(ps.verbose)
                {
                  fmt::print("[i] compute level with node {}\n", action.level[0].first);
                }
                compute_level_with_copies(action.level);
              },
              [&] (uncompute_level_action const& action){
                if(!action.level.empty())
                {
                  if(ps.verbose)
                  {
                    fmt::print("[i] uncompute level with node {}\n", action.level[0].first);
                  }
                  uncompute_level(action.level);
                }
              }},
          action );
    } );

    prepare_outputs();
    return true;
  }

private:
  void prepare_inputs()
  {
    /* prepare primary inputs of logic network */
    ntk.foreach_pi( [&]( auto n ) {
      std::stack<uint32_t> sta;
      sta.push( qnet.num_qubits() );
      node_to_qubit[n] = sta;
      st.i_indexes.push_back( node_to_qubit[n].top() );
      qnet.add_qubit();
    } );
    ntk.foreach_gate( [&]( auto n ) {
      std::stack<uint32_t> sta;
      node_to_qubit[n] = sta;
    } );
  }

  void prepare_constant( bool value )
  {
    const auto f = ntk.get_constant( value );
    const auto n = ntk.get_node( f );
    if ( ntk.fanout_size( n ) == 0 )
      return;
    const auto v = ntk.constant_value( n ) ^ ntk.is_complemented( f );
    node_to_qubit[n].push( qnet.num_qubits() );
    qnet.add_qubit();
    if ( v )
      qnet.add_gate( tweedledum::gate::pauli_x, node_to_qubit[n].top() );
  }

  void record_pipos() {
    ntk.foreach_pi( [&]( auto n ) {
      pis.insert(n);
    } );
    ntk.foreach_po( [&]( auto s ) {
      auto node = ntk.get_node(s);
      pos.insert(node);
    } );
  }

  uint32_t request_ancilla()
  {
    if ( free_ancillae.empty() )
    {
      const auto r = qnet.num_qubits();
      st.required_ancillae++;
      qnet.add_qubit();
      return r;
    }
    else
    {
      const auto r = free_ancillae.top();
      free_ancillae.pop();
      return r;
    }
  }

  uint32_t request_clean_ancilla()
  {
    const auto r = qnet.num_qubits();
    st.required_ancillae++;
    qnet.add_qubit();
    return r;
  }

  void prepare_outputs()
  {
    std::unordered_map<mt::node<LogicNetwork>, mt::signal<LogicNetwork>> node_to_signals;
    ntk.foreach_po( [&]( auto s ) {
      auto node = ntk.get_node( s );

      if ( const auto it = node_to_signals.find( node ); it != node_to_signals.end() || pis.count(node) ) //node previously referred or node is a primary input
      {
        auto new_i = request_clean_ancilla();

        qnet.add_gate( tweedledum::gate::cx, node_to_qubit[ntk.node_to_index( node )].top(), new_i );
        if ( ntk.is_complemented( s ) != ntk.is_complemented( node_to_signals[node] ) )
        {
          qnet.add_gate( tweedledum::gate::pauli_x, new_i );
        }
        st.o_indexes.push_back( new_i );
      }
      else //node never referred
      {
        if ( ntk.is_complemented( s ) )
        {
          qnet.add_gate( tweedledum::gate::pauli_x, node_to_qubit[ntk.node_to_index( node )].top() );
        }
        node_to_signals[node] = s;
        st.o_indexes.push_back( node_to_qubit[ntk.node_to_index( node )].top() );
      }
    } );
  }

  void release_ancilla( uint32_t q )
  {
    free_ancillae.push( q );
  }

  template<int Fanin>
  std::array<uint32_t, Fanin> get_fanin_as_literals( mt::node<LogicNetwork> const& n )
  {
    std::array<uint32_t, Fanin> controls;
    ntk.foreach_fanin( n, [&]( auto const& f, auto i ) {
      controls[i] = ( ntk.node_to_index( ntk.get_node( f ) ) << 1 ) | ntk.is_complemented( f );
    } );
    return controls;
  }

  SetQubits get_fanin_as_qubits( mt::node<LogicNetwork> const& n )
  {
    SetQubits controls;
    ntk.foreach_fanin( n, [&]( auto const& f ) {
      assert( !ntk.is_complemented( f ) );
      controls.push_back( tweedledum::qubit_id( node_to_qubit[ntk.node_to_index( ntk.get_node( f ) )].top() ) );
    } );
    return controls;
  }

  void compute_big_xor( uint32_t const t, std::vector<uint32_t> const leaves)
  {
    for ( auto control : leaves )
    {
      auto c =  node_to_qubit[control].top();
      if (c != t)
      {     
        qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c ), tweedledum::qubit_id( t ) );
      }
    }
  }


  void compute_node( mt::node<LogicNetwork> const& node, uint32_t t)
  {
    if constexpr ( mt::has_is_and_v<LogicNetwork> )
    {
      if ( ntk.is_and( node ) )
      {
        auto controls = get_fanin_as_literals<2>( node );
        auto node0 = ntk.index_to_node( controls[0] >> 1 );
        auto node1 = ntk.index_to_node( controls[1] >> 1 );

        SetQubits pol_controls;     
        pol_controls.emplace_back( node_to_qubit[node0].top(), controls[0] & 1 );
        pol_controls.emplace_back( node_to_qubit[node1].top(), controls[1] & 1 );
        
        compute_and( pol_controls, t );
        return;
      }
    }
    if constexpr ( mt::has_is_or_v<LogicNetwork> )
    {
      if ( ntk.is_or( node ) )
      {
        auto controls = get_fanin_as_literals<2>( node );

        SetQubits pol_controls;
        pol_controls.emplace_back( node_to_qubit[ntk.index_to_node( controls[0] >> 1 )].top(), !( controls[0] & 1 ) );
        pol_controls.emplace_back( node_to_qubit[ntk.index_to_node( controls[1] >> 1 )].top(), !( controls[1] & 1 ) );

        compute_or( pol_controls, t );
        return;
      }
    }
    if constexpr ( mt::has_is_xor_v<LogicNetwork> )
    {
      if ( ntk.is_xor( node ) )
      {
        
        auto controls = get_fanin_as_literals<2>( node );
        compute_xor( node_to_qubit[ntk.index_to_node( controls[0] >> 1 )].top(),
                     node_to_qubit[ntk.index_to_node( controls[1] >> 1 )].top(),
                     ( controls[0] & 1 ) != ( controls[1] & 1 ), t );
        return;
      }
    }
    if constexpr ( mt::has_is_nary_xor_v<LogicNetwork> )
    {
      if ( ntk.is_nary_xor( node ) )
      {
        
        std::vector<uint32_t> controls;
        controls.reserve( ntk.fanin_size(node) );
        ntk.foreach_fanin(node, [&] (auto f)
        {
          controls.push_back(ntk.get_node(f));
        });
        compute_big_xor( t, controls);
        return;
      }
    }
    if constexpr ( mt::has_is_xor3_v<LogicNetwork> )
    {
      if ( ntk.is_xor3( node ) )
      {
        auto controls = get_fanin_as_literals<3>( node );

        /* Is XOR3 in fact an XOR2? */
        if ( ntk.is_constant( ntk.index_to_node( controls[0] >> 1 ) ) )
        {
          compute_xor( node_to_qubit[ntk.index_to_node( controls[1] >> 1 )].top(),
                       node_to_qubit[ntk.index_to_node( controls[2] >> 1 )].top(),
                       ( ( controls[0] & 1 ) != ( controls[1] & 1 ) ) != ( controls[2] & 1 ),
                       t );
        }
        else
        {
          compute_xor3(
              node_to_qubit[ntk.index_to_node( controls[0] >> 1 )].top(),
              node_to_qubit[ntk.index_to_node( controls[1] >> 1 )].top(),
              node_to_qubit[ntk.index_to_node( controls[2] >> 1 )].top(),
              ( ( controls[0] & 1 ) != ( controls[1] & 1 ) ) != ( controls[2] & 1 ),
              t );
        }
        return;
      }
    }
    if constexpr ( mt::has_is_maj_v<LogicNetwork> )
    {
      if ( ntk.is_maj( node ) )
      {
        auto controls = get_fanin_as_literals<3>( node );
        /* Is XOR3 in fact an AND or OR? */
        if ( ntk.is_constant( ntk.index_to_node( controls[0] >> 1 ) ) )
        {
          if ( controls[0] & 1 )
          {
            SetQubits pol_controls;
            pol_controls.emplace_back( node_to_qubit[ntk.index_to_node( controls[1] >> 1 )].top(), !( controls[1] & 1 ) );
            pol_controls.emplace_back( node_to_qubit[ntk.index_to_node( controls[2] >> 1 )].top(), !( controls[2] & 1 ) );

            compute_or( pol_controls, tweedledum::qubit_id( t ) );
          }
          else
          {
            SetQubits pol_controls;
            pol_controls.emplace_back( node_to_qubit[ntk.index_to_node( controls[1] >> 1 )].top(), controls[1] & 1 );
            pol_controls.emplace_back( node_to_qubit[ntk.index_to_node( controls[2] >> 1 )].top(), controls[2] & 1 );

            compute_and( pol_controls, tweedledum::qubit_id( t ) );
          }
        }
        else
        {
          compute_maj(
              node_to_qubit[ntk.index_to_node( controls[0] >> 1 )].top(),
              node_to_qubit[ntk.index_to_node( controls[1] >> 1 )].top(),
              node_to_qubit[ntk.index_to_node( controls[2] >> 1 )].top(),
              controls[0] & 1, controls[1] & 1, controls[2] & 1, t );
        }
        return;
      }
    }
    if constexpr ( mt::has_node_function_v<LogicNetwork> )
    {
      kitty::dynamic_truth_table tt = ntk.node_function( node );
      auto clone = tt.construct();
      kitty::create_parity( clone );

      if ( tt == clone )
      {
        const auto controls = get_fanin_as_qubits( node );
        compute_xor_block( controls, tweedledum::qubit_id( t ) );
      }
      else
      {
        // In this case, the procedure works a bit different and retrieves the
        // controls directly as mapped qubits.  We assume that the inputs cannot
        // be complemented, e.g., in the case of k-LUT networks.
        const auto controls = get_fanin_as_qubits( node );
        compute_lut( ntk.node_function( node ), controls, tweedledum::qubit_id( t ) );
      }
    }
  }

  void compute_node_as_cell( mt::node<LogicNetwork> const& node, uint32_t t, kitty::dynamic_truth_table const& func, std::vector<uint32_t> const& leave_indexes )
  {
    (void)node;

    /* get control qubits */
    SetQubits controls;
    for ( auto l : leave_indexes )
    {
      controls.push_back( tweedledum::qubit_id( node_to_qubit[ntk.node_to_index( l )].top() ) );
    }

    compute_lut( func, controls, tweedledum::qubit_id( t ) );
  }

  void compute_node_inplace( mt::node<LogicNetwork> const& node, uint32_t t )
  {
    if constexpr ( mt::has_is_xor_v<LogicNetwork> )
    {
      if ( ntk.is_xor( node ) )
      {
        auto controls = get_fanin_as_literals<2>( node );
        compute_xor_inplace( node_to_qubit[ntk.index_to_node( controls[0] >> 1 )].top(),
                             node_to_qubit[ntk.index_to_node( controls[1] >> 1 )].top(),
                             ( controls[0] & 1 ) != ( controls[1] & 1 ), t );
        return;
      }
    }
    if constexpr ( mt::has_is_xor3_v<LogicNetwork> )
    {
      if ( ntk.is_xor3( node ) )
      {
        auto controls = get_fanin_as_literals<3>( node );

        /* Is XOR3 in fact an XOR2? */
        if ( ntk.is_constant( ntk.index_to_node( controls[0] >> 1 ) ) )
        {
          compute_xor_inplace(
              node_to_qubit[ntk.index_to_node( controls[1] >> 1 )].top(),
              node_to_qubit[ntk.index_to_node( controls[2] >> 1 )].top(),
              ( ( controls[0] & 1 ) != ( controls[1] & 1 ) ) != ( controls[2] & 1 ),
              t );
        }
        else
        {
          compute_xor3_inplace(
              node_to_qubit[ntk.index_to_node( controls[0] >> 1 )].top(),
              node_to_qubit[ntk.index_to_node( controls[1] >> 1 )].top(),
              node_to_qubit[ntk.index_to_node( controls[2] >> 1 )].top(),
              ( ( controls[0] & 1 ) != ( controls[1] & 1 ) ) != ( controls[2] & 1 ),
              t );
        }
        return;
      }
    }
    if constexpr ( mt::has_node_function_v<LogicNetwork> )
    {
      const auto controls = get_fanin_as_qubits( node );
      compute_xor_block( controls, tweedledum::qubit_id( t ) );
    }
  }

  void compute_and( SetQubits controls, uint32_t t )
  {
    qnet.add_gate( tweedledum::gate::mcx, controls, SetQubits{{t}} );
  }

  void compute_or( SetQubits controls, uint32_t t )
  {
    qnet.add_gate( tweedledum::gate::mcx, controls, SetQubits{{t}} );
    qnet.add_gate( tweedledum::gate::pauli_x, tweedledum::qubit_id( t ) );
  }

  void compute_xor( uint32_t c1, uint32_t c2, bool inv, uint32_t t)
  {
    qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c1 ), tweedledum::qubit_id( t ) );
    qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c2 ), tweedledum::qubit_id( t ) );
    if ( inv )
      qnet.add_gate( tweedledum::gate::pauli_x, tweedledum::qubit_id( t ) );
  }

  void compute_xor3( uint32_t c1, uint32_t c2, uint32_t c3, bool inv, uint32_t t )
  {
    qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c1 ), tweedledum::qubit_id( t ) );
    qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c2 ), tweedledum::qubit_id( t ) );
    qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c3 ), tweedledum::qubit_id( t ) );
    if ( inv )
      qnet.add_gate( tweedledum::gate::pauli_x, tweedledum::qubit_id( t ) );
  }

  void compute_maj( uint32_t c1, uint32_t c2, uint32_t c3, bool p1, bool p2, bool p3, uint32_t t )
  {
    if ( p1 )
      qnet.add_gate( tweedledum::gate::pauli_x, c1 );
    if ( !p2 ) /* control 2 behaves opposite */
      qnet.add_gate( tweedledum::gate::pauli_x, c2 );
    if ( p3 )
      qnet.add_gate( tweedledum::gate::pauli_x, c3 );

    qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c1 ), c2 );
    qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c3 ), c1 );
    qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c3 ), t );

    SetQubits controls;
    controls.push_back( tweedledum::qubit_id( c1 ) );
    controls.push_back( tweedledum::qubit_id( c2 ) );
    qnet.add_gate( tweedledum::gate::mcx, controls, SetQubits{{t}} );

    qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c3 ), c1 );
    qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c1 ), c2 );

    if ( p3 )
      qnet.add_gate( tweedledum::gate::pauli_x, c3 );
    if ( !p2 )
      qnet.add_gate( tweedledum::gate::pauli_x, c2 );
    if ( p1 )
      qnet.add_gate( tweedledum::gate::pauli_x, c1 );
  }

  void compute_xor_block( SetQubits const& controls, Qubit t )
  {
    for ( auto c : controls )
    {
      if ( c != t )
        qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c ), t );
    }
  }

  void compute_lut( kitty::dynamic_truth_table const& function,
                    SetQubits const& controls, Qubit t )
  {
    auto qubit_map = controls;
    qubit_map.push_back( t );
    stg_fn( qnet, qubit_map, function );
  }

  void compute_xor_inplace( uint32_t c1, uint32_t c2, bool inv, uint32_t t )
  {

    if ( c1 == t && c2 != t)
    {
      qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c2 ), c1 );
    }
    else if ( c2 == t && c1 !=t)
    {
      qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c1 ), c2 );
    }
    else if (c1 != t && c2!= t && c1 != c2)
    {
      //std::cerr << "[e] target does not match any control in in-place\n";
      qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c1 ), t );
      qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c2 ), t );
    }
    if ( inv )
      qnet.add_gate( tweedledum::gate::pauli_x, t );
  }

  void compute_xor3_inplace( uint32_t c1, uint32_t c2, uint32_t c3, bool inv, uint32_t t )
  {
    if ( c1 == t )
    {
      qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c2 ), tweedledum::qubit_id( c1 ) );
      qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c3 ), tweedledum::qubit_id( c1 ) );
    }
    else if ( c2 == t )
    {
      qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c1 ), c2 );
      qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c3 ), c2 );
    }
    else if ( c3 == t )
    {
      qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c1 ), c3 );
      qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c2 ), c3 );
    }
    else
    {
      //std::cerr << "[e] target does not match any control in in-place\n";
      qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c1 ), t );
      qnet.add_gate( tweedledum::gate::cx, tweedledum::qubit_id( c2 ), t );
    }
    if ( inv )
      qnet.add_gate( tweedledum::gate::pauli_x, t );
  }

  /*  
      For each node in the level inserts copies and returns the two 
      qubits where the roots of each node are stored.             
  */
  void compute_copies ( std::map<node_t, std::vector<uint32_t>>& id_to_tcp,  caterpillar::level_info_t const& level)
  {
    for(auto node : level)
    { 
      auto &id = node.first;
      if(ntk.is_nary_xor(id))
      {
        std::vector<uint32_t> fanins;
        ntk.foreach_fanin(id, [&] (auto f)
        {
          fanins.push_back(ntk.get_node(f));
        });
        id_to_tcp[id] = fanins;
        continue;
      }     
      std::vector<uint32_t> roots_targets (2);
      for(auto i = 0; i < 2 ; i++)
      {
        auto &cone = node.second[i];
        if(cone.copies.empty())
        {
          if(cone.leaves.size() == 1)
          {
            roots_targets[i] = node_to_qubit[cone.leaves[0]].top();
            continue;
          }
          else
          {
            roots_targets[i] = cone.target.empty() ? request_ancilla() : node_to_qubit[cone.target[0]].top();
          }
        }
        else
        {
          auto tcp = request_ancilla();
          compute_big_xor(tcp, cone.copies);
          roots_targets[i] = tcp;
        }
      }
      id_to_tcp[id] = roots_targets;
    }
  }

  void remove_copies( std::map<node_t, std::vector<uint32_t>>& id_to_tcp, caterpillar::level_info_t  const&  level )
  {
    for(auto node : level)
    {
      if(ntk.is_nary_xor(node.first)) continue;
      for(auto i = 0; i < 2 ; i++)
      {
        auto cone = node.second[i];
        if(cone.copies.empty()) continue;

        auto tcp = id_to_tcp[node.first][i];
        
        for (auto c : cone.copies)
        {
          qnet.add_gate(tweedledum::gate::cx, tweedledum::qubit_id( node_to_qubit[c].top() ), tcp );
        }
        release_ancilla(tcp);
      }
    }
  }

  void compute_and_xor_from_controls(uint32_t id, SetQubits const& pol_controls, uint32_t target)
  {
    if (ntk.is_and(id))
    {
      compute_and(pol_controls, target );
    }
    else if (ntk.is_xor(id))
    {
      compute_xor(pol_controls[0].index(), pol_controls[1].index(), 
                pol_controls[0].is_complemented() != pol_controls[1].is_complemented(), 
                target );
    }
  }

  void compute_level_with_copies(caterpillar::level_info_t const& level)
  {      

    std::map<node_t, std::vector<uint32_t>> id_to_tcp;
    compute_copies(id_to_tcp, level);
    std::vector<uint32_t> qubit_offset;

    
    for(auto node : level)
    {      
      auto &id = node.first;
      auto target = request_ancilla();

      /* nary xor nodes directly point to AND nodes */
      if(ntk.is_nary_xor(id))
      {
        compute_node(id, target);
        node_to_qubit[id].push(target);
        continue;
      }

      SetQubits pol_controls; 
      for(auto i = 0; i < 2 ; i++)
      {
        auto &cone = node.second[i];
        auto &tcp = id_to_tcp[id][i];
        pol_controls.emplace_back(tcp, cone.complemented);

        std::vector<uint32_t> rem_leaves;
        std::set_symmetric_difference( 
                  cone.leaves.begin(), cone.leaves.end(), cone.copies.begin(), cone.copies.end(), 
                  std::back_inserter(rem_leaves) );
        
        compute_big_xor(tcp, rem_leaves);
        node_to_qubit[cone.root].push(tcp);
      }


      //  automatically take into account the extra qubit needed 
      //  for the AND implementation with T-depth = 1
      if (ntk.is_and(id))
      { 
        if (ps.low_tdepth_AND)
        {
          qubit_offset.push_back(request_ancilla());
        }
      }

      compute_and_xor_from_controls(id, pol_controls, target);
      node_to_qubit[id].push(target);

      for(auto i = 1; i >= 0 ; i--)
      {
        auto& cone = node.second[i];
        auto& tcp = id_to_tcp[id][i];

        std::vector<uint32_t> rem_leaves;
        std::set_symmetric_difference( 
                  cone.leaves.begin(), cone.leaves.end(), cone.copies.begin(), cone.copies.end(), 
                  std::back_inserter(rem_leaves) );
        

        compute_big_xor(tcp, rem_leaves);
      }
      node_to_qubit[node.second[0].root].pop();
      node_to_qubit[node.second[1].root].pop();

    }
    assert(qubit_offset.size() <= level.size());
    
    for (auto q : qubit_offset)
      release_ancilla(q);

    remove_copies(id_to_tcp, level);

  }

  void uncompute_level(caterpillar::level_info_t const& level)
  {
    //reverse the orther of the cones
    // only AND nodes are uncomputed
    for(int n = level.size()-1; n >=0; n--)
    {
      SetQubits pol_controls; 

      auto &id = level[n].first;
      
      for(auto i = 0; i < 2 ; i++)
      {
        auto &cone = level[n].second[i];
        
        if(cone.leaves.size() == 1)
        {
          pol_controls.emplace_back(node_to_qubit[cone.leaves[0]].top(), cone.complemented);
          continue;
        } 

        auto t = cone.target.empty() ? request_ancilla() : node_to_qubit[cone.target[0]].top();
        pol_controls.emplace_back(t, cone.complemented);

        compute_big_xor(t, cone.leaves);
        node_to_qubit[cone.root].push(t);
      }

      auto target = node_to_qubit[id].top();
      compute_and_xor_from_controls(id, pol_controls, target);
      node_to_qubit[id].pop();

      for(int i = 1; i >= 0 ; i--)
      {
        auto &cone = level[n].second[i];
        if(cone.leaves.size() == 1) continue;

        auto t = node_to_qubit[cone.root].top();

        compute_big_xor(t, cone.leaves);
        node_to_qubit[cone.root].pop();
      }
    }

  }


private:
  QuantumNetwork& qnet;
  LogicNetwork const& ntk;
  mapping_strategy<LogicNetwork>& strategy;
  SingleTargetGateSynthesisFn const& stg_fn;
  logic_network_synthesis_params const& ps;
  logic_network_synthesis_stats& st;
  std::unordered_map<uint32_t, std::stack<uint32_t>> node_to_qubit;
  std::stack<uint32_t> free_ancillae;
  std::set<uint32_t> pis;
  std::set<uint32_t> pos;
  /* stores for each root of the cone a queue of qubits where its copies are and its previous location */
  std::unordered_map<uint32_t, std::queue<uint32_t>> copies;
}; // namespace detail

}

template<class QuantumNetwork, class LogicNetwork,
         class SingleTargetGateSynthesisFn = tweedledum::stg_from_pprm>
bool logic_network_synthesis_oracle( QuantumNetwork& qnet, LogicNetwork const& ntk,
                              mapping_strategy<LogicNetwork>& strategy,
                              SingleTargetGateSynthesisFn const& stg_fn = {},
                              logic_network_synthesis_params const& ps = {},
                              logic_network_synthesis_stats* pst = nullptr )
{
  static_assert( mt::is_network_type_v<LogicNetwork>, "LogicNetwork is not a network type" );

  logic_network_synthesis_stats st;
  detail::logic_network_synthesis_impl_oracle<QuantumNetwork, LogicNetwork, SingleTargetGateSynthesisFn> impl( qnet,
                                                                                                        ntk,
                                                                                                        strategy,
                                                                                                        stg_fn,
                                                                                                        ps, st );
  const auto result = impl.run();
  if ( ps.verbose )
  {
    st.report();
  }

  if ( pst )
  {
    *pst = st;
  }

  return result;
}

}
