use clap::{App, Arg, SubCommand};

fn main() {
    let app = App::new("manage")
        .version("1.0")
        .author("Nicholas Bailey <nicholas.j.bailey91@gmail.com>")
        .about("Manage the Emerald server")
        .subcommand(
            SubCommand::with_name("accounts").subcommand(
                SubCommand::with_name("create")
                    .help("Create a new account")
                    .version("1.0")
                    .arg(
                        Arg::with_name("username")
                            .short("u")
                            .takes_value(true)
                            .required(true),
                    )
                    .arg(
                        Arg::with_name("password")
                            .short("p")
                            .takes_value(true)
                            .required(true),
                    )
                    .arg(
                        Arg::with_name("status")
                            .short("s")
                            .default_value("0")
                            .validator(|value| match value.parse::<i16>() {
                                Ok(_) => Ok(()),
                                Err(_) => Err("Please enter an integer value.".to_string()),
                            }),
                    ),
            ),
        )
        .get_matches();

    if let Some(matches) = app.subcommand_matches("accounts") {
        if let Some(matches) = matches.subcommand_matches("create") {
            let conn = database::establish_connection();

            use database::models::Account;

            let username = matches.value_of("username").unwrap();
            let password = matches.value_of("password").unwrap();
            let status = matches.value_of("status").unwrap().parse().unwrap();

            let account = Account::create(&conn, username, password, Some(status));

            if let Some(account) = account {
                println!(
                    "Successfully created account with username \"{}\"",
                    account.username
                );
            } else {
                println!("The username \"{}\" already exists.", username);
            }
        }
    }
}
