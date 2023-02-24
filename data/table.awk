#!/usr/bin/awk

BEGIN {
    # Begin a transaction
    print "BEGIN TRANSACTION;"

    # Create english table
    print "CREATE TABLE IF NOT EXISTS phrases ( "      \
        "id INTEGER PRIMARY KEY NOT NULL,"             \
        "tabkeys TEXT NOT NULL,"                       \
        "phrase TEXT NOT NULL,"                        \
        "freq INTEGER NOT NULL DEFAULT (10)"           \
        ");";

    # Create desc table
    print "CREATE TABLE IF NOT EXISTS desc (name TEXT PRIMARY KEY, value TEXT);";
    print "INSERT OR IGNORE INTO desc VALUES ('version', '1.12.0');";

    id = 1;
}

# Insert data into phrases table
NF == 4 {
    printf "INSERT INTO phrases (id, tabkeys, phrase) VALUES (%d, '%s', '%s');\n", id, $3, $1;
    id++;
}

#quit sqlite3
END {
    # Commit the transcation
    print "COMMIT;"
}
