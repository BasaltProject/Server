#include <basalt/server/storage/Schema.hpp>

namespace basalt::server::storage::schema {

std::string_view ddl() {
    return R"(
-- One row per player, keyed by UUID, in a database that lives outside every world directory.
--
-- Vanilla writes this state into the world folder (playerdata/<uuid>.dat), which is what makes a
-- world impossible to delete or reset without destroying player state along with it, and what makes
-- per-world inventories a plugin's problem. Keeping it here is the whole point of the layout.
--
-- Indexable facts are columns; anything with nested structure (items and their enchantments, custom
-- NBT) is a single binary blob on the same row. Splitting an inventory into one row per slot buys
-- joins and no queries anyone actually runs: it is always read and written whole.
CREATE TABLE IF NOT EXISTS player (
    uuid TEXT NOT NULL PRIMARY KEY,
    name TEXT NOT NULL,
    last_seen INTEGER NOT NULL,
    first_seen INTEGER NOT NULL,
    play_time_ticks INTEGER NOT NULL DEFAULT 0,
    world TEXT NOT NULL,
    x REAL NOT NULL,
    y REAL NOT NULL,
    z REAL NOT NULL,
    yaw REAL NOT NULL,
    pitch REAL NOT NULL,
    health REAL NOT NULL,
    food INTEGER NOT NULL,
    saturation REAL NOT NULL,
    exhaustion REAL NOT NULL,
    xp_level INTEGER NOT NULL,
    xp_progress REAL NOT NULL,
    xp_total INTEGER NOT NULL,
    gamemode INTEGER NOT NULL,
    -- Which permission group they are in. Null means whichever group is currently the default,
    -- resolved at read time so renaming that group does not mean rewriting every row.
    group_name TEXT,
    -- The address they last logged in from, and only that: it is written when the server list
    -- greeting is on, so the list can say hello by name. Never read by anything that decides what a
    -- player may do, because an address identifies a router, not a person.
    last_ip TEXT,
    inventory BLOB,
    ender_chest BLOB,
    stats BLOB,
    -- Scoreboard tags, comma-separated. A list rather than a table because it is always read and
    -- written whole, exactly like the inventory blob above it.
    tags TEXT,
    -- The screen shaders /posteffect put on them, comma-separated, in the order they were added.
    post_effects TEXT
);

-- Name lookup is how an operator refers to a player; the others are what the admin queries scan.
CREATE INDEX IF NOT EXISTS player_name ON player(name);
CREATE INDEX IF NOT EXISTS player_last_seen ON player(last_seen);
CREATE INDEX IF NOT EXISTS player_world ON player(world);
-- Looked up on every server list ping that greets by name, which is a request anyone can send.
CREATE INDEX IF NOT EXISTS player_last_ip ON player(last_ip);

-- Who may connect: bans by player, bans by address, and the whitelist. In the same database as
-- players and not in config/, because these are state the server writes, not configuration the
-- operator maintains by hand.
CREATE TABLE IF NOT EXISTS ban (
    -- The player's UUID, or the address for an IP ban. One table, because both answer the same
    -- question at login and a two-table version needs the same union query anyway.
    subject TEXT NOT NULL PRIMARY KEY,
    -- 'player' or 'ip', so a UUID that looks like an address cannot collide with one.
    kind TEXT NOT NULL,
    name TEXT,
    reason TEXT NOT NULL,
    source TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    -- Null means it never expires, which is every ban vanilla's own commands create.
    expires_at INTEGER
);

CREATE INDEX IF NOT EXISTS ban_kind ON ban(kind);

CREATE TABLE IF NOT EXISTS whitelist (
    uuid TEXT NOT NULL PRIMARY KEY,
    name TEXT NOT NULL,
    added_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS whitelist_name ON whitelist(name);

-- Where a player respawns, as /spawnpoint set it. Its own table rather than columns on player,
-- because it is written by a command that may name somebody who has never joined, and the
-- whole-player upsert would then have to invent the rest of their row. A missing row means
-- "wherever the world spawns", which is the vanilla default.
CREATE TABLE IF NOT EXISTS spawn_point (
    uuid TEXT NOT NULL PRIMARY KEY,
    world TEXT NOT NULL,
    x REAL NOT NULL,
    y REAL NOT NULL,
    z REAL NOT NULL,
    yaw REAL NOT NULL
);

-- What a player has been granted and taught. Vanilla keeps advancement progress in
-- advancements/<uuid>.json inside the world folder and the recipe book inside the player's .dat, so
-- both die with the world. One row per criterion rather than a blob per player, because that is the
-- unit /advancement grant writes, and because "who has this advancement" is then a query rather
-- than a scan that deserializes everyone.
CREATE TABLE IF NOT EXISTS advancement_criterion (
    uuid TEXT NOT NULL,
    advancement TEXT NOT NULL,
    criterion TEXT NOT NULL,
    PRIMARY KEY (uuid, advancement, criterion)
);

CREATE TABLE IF NOT EXISTS known_recipe (
    uuid TEXT NOT NULL,
    recipe TEXT NOT NULL,
    PRIMARY KEY (uuid, recipe)
);

-- What plugins keep about players, one namespace each. Keyed by the plugin's id: a plugin can only
-- ever see rows with its own id in them, so two of them cannot read or overwrite each other by
-- accident, and neither can touch the columns the server itself depends on. Uninstalling a plugin
-- leaves its rows here on purpose, which is what the orphan warning at startup is about.
CREATE TABLE IF NOT EXISTS plugin_data (
    plugin TEXT NOT NULL,
    uuid TEXT NOT NULL,
    key TEXT NOT NULL,
    value TEXT NOT NULL,
    PRIMARY KEY (plugin, uuid, key)
);

CREATE INDEX IF NOT EXISTS plugin_data_by_plugin ON plugin_data(plugin);
)";
}

} // namespace basalt::server::storage::schema
