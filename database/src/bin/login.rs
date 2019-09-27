use database::models::*;

fn main() {
    let connection = database::establish_connection();

    let account = Account::with_credentials(&connection, "nick", "nick");

    if let Some(_account) = account {
        println!("Successfully logged in.");
    } else {
        println!("Incorrect username/password.");
    }
}
