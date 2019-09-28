#![allow(dead_code)]

#[macro_use]
extern crate log;

use database;
use database::models::Account;

use rand::Rng;
use std::collections::HashMap;

mod session;
use session::Session;

const SESSION_ID_LENGTH: usize = 28;

struct Api {
    db: diesel::pg::PgConnection,
    session_map: HashMap<String, Session>
}

impl Api {
    pub fn new() -> Self {
        Self {
            db: database::establish_connection(),
            session_map: HashMap::new(),
        }
    }

    pub fn login<S: AsRef<str>>(&mut self, username: S, password: S) -> Option<String> {
        let account = Account::with_credentials(&self.db, username, password);

        if let Some(account) = account {
            let sid = generate_session_id();

            let session = Session::new( account.id as u32, sid.clone());
            self.session_map.insert(sid.clone(), session);

            Some(sid)
        } else {
            None
        }
    }

    pub fn create<S: AsRef<str>>(&mut self, username: S, password: S) -> Option<String> {
        let account = Account::with_credentials(&self.db, username.as_ref(), password.as_ref());

        if account.is_some() {
            return None;
        }

        let account = Account::create(&self.db, username.as_ref(), password.as_ref(), None);

        if let Some(account) = account {
            self.login(account.username, account.password)
        } else {
            error!("Unable to create account.");
            None
        }
    }

    pub fn get_account_id_for_session<S: AsRef<str>>(&self, sid: S) -> Option<u32> {
        if let Some(session) = self.session_map.get(sid.as_ref()) {
            Some(session.account_id())
        } else {
            None
        }
    }

    pub fn remove_session<S: AsRef<str>>(&mut self, sid: S) {
        self.session_map.remove(sid.as_ref());
    }
}

fn generate_session_id() -> String {
    let mut sid = String::new();
    let mut rng = rand::thread_rng();

    for _ in 0..SESSION_ID_LENGTH/2 {
        let number = 0x1111 + rng.gen::<u16>() as u32;
        sid.push_str(&format!("{:04X}", number as u16));
    }

    sid
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_session_id_generation() {
        let sid = generate_session_id();

        assert_eq!(sid.len() / 2, SESSION_ID_LENGTH);
    }
}