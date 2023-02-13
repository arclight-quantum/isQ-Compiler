#[macro_use]
extern crate nom;
#[macro_use]
extern crate nom_locate;

pub mod lang;

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        let result = 2 + 2;
        assert_eq!(result, 4);
    }
}
