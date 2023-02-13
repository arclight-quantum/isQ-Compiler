#[derive(Copy, Clone, Debug)]
pub struct Span{
    pub byte_offset: usize,
    pub byte_len: usize,
}
impl Span{
    pub fn span_over(self, rhs: Self)->Self{
        Span{
            byte_offset: self.byte_offset,
            byte_len: rhs.byte_offset + rhs.byte_len - self.byte_offset
        }
    }
}