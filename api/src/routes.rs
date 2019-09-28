use crate::connection::Connection;
use database::models::Account;

use rocket_contrib::json::Json;

#[get("/accounts")]
pub fn get_accounts(conn: Connection) -> Json<Vec<Account>> {
    Json(Account::all(&conn))
}
