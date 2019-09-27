use chrono::NaiveDateTime;
use crate::schema::*;
use diesel::{RunQueryDsl, PgConnection};

#[derive(Debug, Queryable)]
pub struct Account {
    pub id: i32,
    pub name: String,
    pub pass: String,
    pub created: NaiveDateTime,
    pub status: i16,
}

impl Account {
    pub fn create<S: AsRef<str>>(conn: &PgConnection, name: S, password: S, status: Option<i16>) -> Account {
        let new_account = NewAccount {
            name: name.as_ref(),
            password: password.as_ref(),
            status
        };

        diesel::insert_into(accounts::table)
            .values(new_account)
            .get_result(conn)
            .expect("Unable to create new account.")
    }
}

#[derive(Insertable)]
#[table_name="accounts"]
struct NewAccount<'a> {
    pub name: &'a str,
    pub password: &'a str,
    pub status: Option<i16>,
}