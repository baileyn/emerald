use chrono::NaiveDateTime;
use diesel::prelude::*;
use diesel::{PgConnection, RunQueryDsl};

use crate::schema::*;

#[derive(Debug, Queryable)]
pub struct Account {
    pub id: i32,
    pub username: String,
    pub password: String,
    pub created: NaiveDateTime,
    pub status: i16,
}

impl Account {
    pub fn validate_password<S: AsRef<str>>(&self, password: S) -> bool {
        bcrypt::verify(password.as_ref(), &self.password).unwrap_or(false)
    }

    pub fn with_credentials<S: AsRef<str>>(
        conn: &PgConnection,
        username: S,
        password: S,
    ) -> Option<Account> {
        Account::from_username(conn, username.as_ref())
            .filter(|account| account.validate_password(password.as_ref()))
    }

    pub fn from_username<S: AsRef<str>>(conn: &PgConnection, username: S) -> Option<Account> {
        let accounts: Vec<Account> = accounts::dsl::accounts
            .filter(accounts::dsl::username.eq(username.as_ref()))
            .load::<Account>(conn)
            .expect("Unable to load accounts");

        if accounts.len() > 1 {
            panic!("Multiple users returned in query?");
        }

        accounts.into_iter().next()
    }

    pub fn all(conn: &PgConnection) -> Vec<Account> {
        accounts::dsl::accounts
            .load::<Account>(conn)
            .expect("Unable to load accounts.")
    }

    pub fn create<S: AsRef<str>>(
        conn: &PgConnection,
        name: S,
        password: S,
        status: Option<i16>,
    ) -> Option<Account> {
        let new_account = NewAccount {
            username: name.as_ref(),
            password: &bcrypt::hash(password.as_ref(), 8).unwrap(),
            status,
        };

        if let Ok(account) = diesel::insert_into(accounts::table)
            .values(new_account)
            .get_result(conn)
        {
            Some(account)
        } else {
            None
        }
    }
}

#[derive(Insertable)]
#[table_name = "accounts"]
struct NewAccount<'a> {
    pub username: &'a str,
    pub password: &'a str,
    pub status: Option<i16>,
}
