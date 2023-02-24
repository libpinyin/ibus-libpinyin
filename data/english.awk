#!/usr/bin/awk

BEGIN {
    # Begin a transaction
    print "BEGIN TRANSACTION;"

    # Create english table
    print "CREATE TABLE IF NOT EXISTS 'english' ( "   \
        "'word' TEXT NOT NULL PRIMARY KEY,"           \
        "'freq' FLOAT NOT NULL DEFAULT(0)"            \
        ");";

    # Create desc table
    print "CREATE TABLE IF NOT EXISTS desc (name TEXT PRIMARY KEY, value TEXT);";
    print "INSERT OR IGNORE INTO desc VALUES ('version', '1.2.0');";
}

    # Insert data into english table
    {   printf "INSERT INTO english (word, freq) VALUES ('%s', %f);\n", $1, $2}

    #quit sqlite3
END {
    # Commit the transcation
    print "COMMIT;"
}
