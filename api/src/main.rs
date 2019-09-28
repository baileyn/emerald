#![feature(proc_macro_hygiene)]
#![feature(decl_macro)]

#[macro_use]
extern crate rocket;

mod connection;
use connection::Connection;

use database::models::Account;
use rocket_contrib::json::Json;

#[get("/accounts")]
fn get_accounts(conn: Connection) -> Json<Vec<Account>> {
    Json(Account::all(&conn))
}

#[get("/")]
fn index() -> &'static str {
    "Hello, World"
}

fn rocket() -> rocket::Rocket {
    rocket::ignite()
        .manage(connection::connect())
        .mount("/", routes![index, get_accounts])
}

fn main() {
    rocket().launch();
}
