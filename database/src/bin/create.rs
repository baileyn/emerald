use database::models::*;

fn main() {
    let connection = database::establish_connection();

    Account::create(&connection, "nick", "nick", Some(2));

    Account::all(&connection)
        .iter()
        .for_each(|act| println!("{:#?}", act));
}
