#![feature(proc_macro_hygiene)]
#![feature(decl_macro)]

#[macro_use]
extern crate rocket;

mod connection;
mod routes;

fn rocket() -> rocket::Rocket {
    rocket::ignite()
        .manage(connection::connect())
        .mount("/api", routes![routes::get_accounts])
}

fn main() {
    rocket().launch();
}
