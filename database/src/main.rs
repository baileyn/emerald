use diesel::prelude::*;
use database::models::*;

fn main() {
    use database::schema::accounts::dsl::*;

    let connection = database::establish_connection();
    let results = accounts.load::<Account>(&connection)
        .expect("Error loading accounts.");

    println!("Displaying {} accounts", results.len());
    for account in results {
        println!("{:#?}", account);
    }

    println!("Adding a random account.");
    let account = Account::create(&connection, "Username", "Password", Some(0));

    println!("Created account.");
    println!("{:#?}", account);
}
