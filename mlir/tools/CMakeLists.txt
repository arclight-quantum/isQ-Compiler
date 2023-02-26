function(isq_tool tool_name)
add_executable(isq-${tool_name} ${tool_name}.cpp)
target_link_libraries(isq-${tool_name} isqir ${mlir_libs})
add_custom_command(TARGET isq-${tool_name} POST_BUILD
    COMMAND ${CMAKE_STRIP} isq-${tool_name})
install(TARGETS isq-${tool_name} DESTINATION bin)
endfunction()

isq_tool(opt)
isq_tool(example)
#isq_tool(lsp-server)
#isq_tool(ok)
#isq_tool(codegen)
# qir_tool(lower)