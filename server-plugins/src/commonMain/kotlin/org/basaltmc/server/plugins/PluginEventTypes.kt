package org.basaltmc.server.plugins

/**
 * The events this server fires, and what each one carries.
 *
 * Names are dotted and read as `thing.happening`. There are far fewer of these than Bukkit has
 * classes, and on purpose: what Bukkit splits into a dozen subclasses is one event with a field
 * here (`entity.damage` with `cause`, not twelve `EntityDamageBy*Event`). Which Bukkit and Paper
 * event each one answers for is audited in `docs/plan/api-vs-bukkit.md`.
 *
 * Two conventions worth knowing before reading the list. A player is carried as `player_name` and
 * `player_uuid` rather than as an object, because the object model is the next stage and the name
 * `player` is being kept free for it. And a position is `world`, `x`, `y`, `z`, in blocks.
 */
public object PluginEventTypes {
    public val PLAYER_JOIN: PluginEventType =
        PluginEventType(
            name = "player.join",
            cancellable = false,
            fields = listOf("player_name", "player_uuid", "world"),
        )

    public val PLAYER_QUIT: PluginEventType =
        PluginEventType(
            name = "player.quit",
            cancellable = false,
            fields = listOf("player_name", "player_uuid", "world"),
        )

    public val PLAYER_CHAT: PluginEventType =
        PluginEventType(
            name = "player.chat",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "message"),
            mutable = setOf("message"),
        )

    public val PLAYER_COMMAND: PluginEventType =
        PluginEventType(
            name = "player.command",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "command"),
            mutable = setOf("command"),
        )

    public val PLAYER_DEATH: PluginEventType =
        PluginEventType(
            name = "player.death",
            cancellable = false,
            fields = listOf("player_name", "player_uuid", "world", "x", "y", "z", "cause"),
        )

    public val BLOCK_BREAK: PluginEventType =
        PluginEventType(
            name = "block.break",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "x", "y", "z", "block"),
        )

    public val BLOCK_PLACE: PluginEventType =
        PluginEventType(
            name = "block.place",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "x", "y", "z", "block"),
        )

    public val ENTITY_DAMAGE: PluginEventType =
        PluginEventType(
            name = "entity.damage",
            cancellable = true,
            fields = listOf("entity_id", "entity_type", "world", "cause", "amount", "attacker_name"),
            mutable = setOf("amount"),
        )

    public val ENTITY_DEATH: PluginEventType =
        PluginEventType(
            name = "entity.death",
            cancellable = false,
            fields = listOf("entity_id", "entity_type", "world", "x", "y", "z", "killer_name"),
        )

    public val SERVER_TICK: PluginEventType =
        PluginEventType(
            name = "server.tick",
            cancellable = false,
            fields = listOf("tick"),
        )

    public val PLAYER_MOVE: PluginEventType =
        PluginEventType(
            name = "player.move",
            cancellable = true,
            fields =
                listOf(
                    "player_name",
                    "player_uuid",
                    "world",
                    "from_x",
                    "from_y",
                    "from_z",
                    "x",
                    "y",
                    "z",
                ),
        )

    public val PLAYER_TELEPORT: PluginEventType =
        PluginEventType(
            name = "player.teleport",
            cancellable = true,
            fields =
                listOf(
                    "player_name",
                    "player_uuid",
                    "world",
                    "from_x",
                    "from_y",
                    "from_z",
                    "x",
                    "y",
                    "z",
                    "cause",
                ),
        )

    public val PLAYER_WORLD_CHANGE: PluginEventType =
        PluginEventType(
            name = "player.world_change",
            cancellable = false,
            fields = listOf("player_name", "player_uuid", "world", "from_world"),
        )

    public val PLAYER_RESPAWN: PluginEventType =
        PluginEventType(
            name = "player.respawn",
            cancellable = false,
            fields = listOf("player_name", "player_uuid", "world", "x", "y", "z"),
        )

    public val PLAYER_INTERACT: PluginEventType =
        PluginEventType(
            name = "player.interact",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "action", "x", "y", "z", "block", "item", "hand"),
        )

    public val PLAYER_INTERACT_ENTITY: PluginEventType =
        PluginEventType(
            name = "player.interact_entity",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "entity_id", "entity_type", "action", "hand"),
        )

    public val PLAYER_ITEM_DROP: PluginEventType =
        PluginEventType(
            name = "player.item_drop",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "item", "count"),
        )

    public val PLAYER_ITEM_PICKUP: PluginEventType =
        PluginEventType(
            name = "player.item_pickup",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "item", "count", "entity_id"),
        )

    public val PLAYER_ITEM_HELD: PluginEventType =
        PluginEventType(
            name = "player.item_held",
            cancellable = false,
            fields = listOf("player_name", "player_uuid", "world", "slot", "from_slot"),
        )

    public val PLAYER_ITEM_CONSUME: PluginEventType =
        PluginEventType(
            name = "player.item_consume",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "item"),
        )

    public val PLAYER_SWAP_HANDS: PluginEventType =
        PluginEventType(
            name = "player.swap_hands",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "main_item", "off_item"),
        )

    public val PLAYER_SNEAK: PluginEventType =
        PluginEventType(
            name = "player.sneak",
            cancellable = false,
            fields = listOf("player_name", "player_uuid", "world", "sneaking"),
        )

    public val PLAYER_GAMEMODE_CHANGE: PluginEventType =
        PluginEventType(
            name = "player.gamemode_change",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "gamemode", "from_gamemode"),
        )

    public val PLAYER_FOOD_CHANGE: PluginEventType =
        PluginEventType(
            name = "player.food_change",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "food", "from_food"),
            mutable = setOf("food"),
        )

    public val PLAYER_LEVEL_CHANGE: PluginEventType =
        PluginEventType(
            name = "player.level_change",
            cancellable = false,
            fields = listOf("player_name", "player_uuid", "world", "level", "from_level"),
        )

    public val PLAYER_SIGN_CHANGE: PluginEventType =
        PluginEventType(
            name = "player.sign_change",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "x", "y", "z", "line1", "line2", "line3", "line4"),
            mutable = setOf("line1", "line2", "line3", "line4"),
        )

    public val PLAYER_BUCKET: PluginEventType =
        PluginEventType(
            name = "player.bucket",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "x", "y", "z", "item", "filling"),
        )

    public val BLOCK_EXPLODE: PluginEventType =
        PluginEventType(
            name = "block.explode",
            cancellable = true,
            fields = listOf("world", "x", "y", "z", "power", "cause", "entity_id"),
        )

    public val BLOCK_IGNITE: PluginEventType =
        PluginEventType(
            name = "block.ignite",
            cancellable = true,
            fields = listOf("world", "x", "y", "z", "cause", "player_name"),
        )

    public val BLOCK_GROW: PluginEventType =
        PluginEventType(
            name = "block.grow",
            cancellable = true,
            fields = listOf("world", "x", "y", "z", "block", "to_block", "cause"),
        )

    public val BLOCK_DISPENSE: PluginEventType =
        PluginEventType(
            name = "block.dispense",
            cancellable = true,
            fields = listOf("world", "x", "y", "z", "item"),
        )

    public val BLOCK_PISTON: PluginEventType =
        PluginEventType(
            name = "block.piston",
            cancellable = true,
            fields = listOf("world", "x", "y", "z", "extending", "direction", "moved"),
        )

    public val WORLD_LOAD: PluginEventType =
        PluginEventType(name = "world.load", cancellable = false, fields = listOf("world"))

    public val WORLD_SAVE: PluginEventType =
        PluginEventType(name = "world.save", cancellable = false, fields = listOf("world"))

    public val WORLD_CHUNK_LOAD: PluginEventType =
        PluginEventType(
            name = "world.chunk_load",
            cancellable = false,
            fields = listOf("world", "chunk_x", "chunk_z", "generated"),
        )

    public val WORLD_CHUNK_UNLOAD: PluginEventType =
        PluginEventType(
            name = "world.chunk_unload",
            cancellable = false,
            fields = listOf("world", "chunk_x", "chunk_z"),
        )

    public val WORLD_WEATHER_CHANGE: PluginEventType =
        PluginEventType(
            name = "world.weather_change",
            cancellable = true,
            fields = listOf("world", "raining", "thundering", "from_raining", "from_thundering"),
        )

    public val WORLD_LIGHTNING: PluginEventType =
        PluginEventType(
            name = "world.lightning",
            cancellable = true,
            fields = listOf("world", "x", "y", "z"),
        )

    public val WORLD_TIME_SKIP: PluginEventType =
        PluginEventType(
            name = "world.time_skip",
            cancellable = true,
            fields = listOf("world", "time", "from_time", "cause"),
        )

    public val WORLD_GAME_RULE_CHANGE: PluginEventType =
        PluginEventType(
            name = "world.game_rule_change",
            cancellable = true,
            fields = listOf("world", "rule", "value", "from_value", "player_name"),
        )

    public val WORLD_DIFFICULTY_CHANGE: PluginEventType =
        PluginEventType(
            name = "world.difficulty_change",
            cancellable = true,
            fields = listOf("world", "difficulty", "from_difficulty", "player_name"),
        )

    public val SERVER_LOAD: PluginEventType =
        PluginEventType(name = "server.load", cancellable = false, fields = listOf("worlds", "plugins"))

    public val SERVER_STOP: PluginEventType =
        PluginEventType(name = "server.stop", cancellable = false, fields = listOf("players"))

    public val SERVER_PING: PluginEventType =
        PluginEventType(
            name = "server.ping",
            cancellable = false,
            fields = listOf("address", "motd", "online", "max_players"),
            mutable = setOf("motd", "max_players"),
        )

    public val SERVER_COMMAND: PluginEventType =
        PluginEventType(
            name = "server.command",
            cancellable = true,
            fields = listOf("source", "command"),
            mutable = setOf("command"),
        )

    public val SERVER_BROADCAST: PluginEventType =
        PluginEventType(
            name = "server.broadcast",
            cancellable = true,
            fields = listOf("message"),
            mutable = setOf("message"),
        )

    public val ENTITY_SPAWN: PluginEventType =
        PluginEventType(
            name = "entity.spawn",
            cancellable = true,
            fields = listOf("entity_type", "world", "x", "y", "z", "reason"),
        )

    public val ENTITY_TAME: PluginEventType =
        PluginEventType(
            name = "entity.tame",
            cancellable = true,
            fields = listOf("entity_id", "entity_type", "world", "player_name", "player_uuid"),
        )

    public val ENTITY_BREED: PluginEventType =
        PluginEventType(
            name = "entity.breed",
            cancellable = true,
            fields = listOf("entity_id", "entity_type", "world", "partner_id"),
        )

    public val ENTITY_MOUNT: PluginEventType =
        PluginEventType(
            name = "entity.mount",
            cancellable = true,
            fields = listOf("entity_id", "entity_type", "world", "player_name", "player_uuid", "mounting"),
        )

    public val ENTITY_HEAL: PluginEventType =
        PluginEventType(
            name = "entity.heal",
            cancellable = true,
            fields = listOf("entity_id", "entity_type", "world", "amount", "cause"),
            mutable = setOf("amount"),
        )

    public val ENTITY_SHOOT: PluginEventType =
        PluginEventType(
            name = "entity.shoot",
            cancellable = true,
            fields = listOf("entity_id", "entity_type", "world", "projectile", "power"),
        )

    public val ITEM_SPAWN: PluginEventType =
        PluginEventType(
            name = "item.spawn",
            cancellable = true,
            fields = listOf("world", "x", "y", "z", "item", "count", "cause"),
        )

    public val PLAYER_LOGIN: PluginEventType =
        PluginEventType(
            name = "player.login",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "address"),
        )

    public val PLAYER_BED: PluginEventType =
        PluginEventType(
            name = "player.bed",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "x", "y", "z", "entering"),
        )

    public val PLAYER_EFFECT: PluginEventType =
        PluginEventType(
            name = "player.effect",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "effect", "amplifier", "duration"),
        )

    public val INVENTORY_OPEN: PluginEventType =
        PluginEventType(
            name = "inventory.open",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "kind", "x", "y", "z"),
        )

    public val INVENTORY_CLOSE: PluginEventType =
        PluginEventType(
            name = "inventory.close",
            cancellable = false,
            fields = listOf("player_name", "player_uuid", "world", "kind"),
        )

    public val INVENTORY_CLICK: PluginEventType =
        PluginEventType(
            name = "inventory.click",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "slot", "button", "mode", "item"),
        )

    public val INVENTORY_SMELT: PluginEventType =
        PluginEventType(
            name = "inventory.smelt",
            cancellable = true,
            fields = listOf("world", "x", "y", "z", "cooker", "item", "result"),
        )

    public val INVENTORY_MOVE_ITEM: PluginEventType =
        PluginEventType(
            name = "inventory.move_item",
            cancellable = true,
            fields = listOf("world", "x", "y", "z", "kind"),
        )

    public val INVENTORY_TRADE: PluginEventType =
        PluginEventType(
            name = "inventory.trade",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "entity_id", "index", "result"),
        )

    public val BLOCK_DAMAGE: PluginEventType =
        PluginEventType(
            name = "block.damage",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "x", "y", "z", "block"),
        )

    public val PLAYER_ITEM_DAMAGE: PluginEventType =
        PluginEventType(
            name = "player.item_damage",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "item", "damage", "broken"),
        )

    public val PLAYER_SPRINT: PluginEventType =
        PluginEventType(
            name = "player.sprint",
            cancellable = false,
            fields = listOf("player_name", "player_uuid", "world", "sprinting"),
        )

    public val PLAYER_PICK_ITEM: PluginEventType =
        PluginEventType(
            name = "player.pick_item",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "x", "y", "z", "item"),
        )

    public val PLAYER_PORTAL: PluginEventType =
        PluginEventType(
            name = "player.portal",
            cancellable = true,
            fields = listOf("player_name", "player_uuid", "world", "to_world", "x", "y", "z", "kind"),
        )

    public val RAID_FINISH: PluginEventType =
        PluginEventType(
            name = "raid.finish",
            cancellable = false,
            fields = listOf("world", "x", "y", "z", "waves", "won"),
        )

    public val ENTITY_COMBUST: PluginEventType =
        PluginEventType(
            name = "entity.combust",
            cancellable = true,
            fields = listOf("entity_id", "entity_type", "world", "ticks", "cause"),
        )

    public val BLOCK_FADE: PluginEventType =
        PluginEventType(
            name = "block.fade",
            cancellable = true,
            fields = listOf("world", "x", "y", "z", "block", "to_block", "cause"),
        )

    /** Every type above, which is what the bus is told about and what the audit walks. */
    public val ALL: List<PluginEventType> =
        listOf(
            PLAYER_JOIN,
            PLAYER_QUIT,
            PLAYER_CHAT,
            PLAYER_COMMAND,
            PLAYER_DEATH,
            PLAYER_MOVE,
            PLAYER_TELEPORT,
            PLAYER_WORLD_CHANGE,
            PLAYER_RESPAWN,
            PLAYER_INTERACT,
            PLAYER_INTERACT_ENTITY,
            PLAYER_ITEM_DROP,
            PLAYER_ITEM_PICKUP,
            PLAYER_ITEM_HELD,
            PLAYER_ITEM_CONSUME,
            PLAYER_SWAP_HANDS,
            PLAYER_SNEAK,
            PLAYER_GAMEMODE_CHANGE,
            PLAYER_FOOD_CHANGE,
            PLAYER_LEVEL_CHANGE,
            PLAYER_SIGN_CHANGE,
            PLAYER_BUCKET,
            BLOCK_BREAK,
            BLOCK_PLACE,
            ENTITY_DAMAGE,
            ENTITY_DEATH,
            SERVER_TICK,
            BLOCK_EXPLODE,
            BLOCK_IGNITE,
            BLOCK_GROW,
            BLOCK_DISPENSE,
            BLOCK_PISTON,
            WORLD_LOAD,
            WORLD_SAVE,
            WORLD_CHUNK_LOAD,
            WORLD_CHUNK_UNLOAD,
            WORLD_WEATHER_CHANGE,
            WORLD_LIGHTNING,
            WORLD_TIME_SKIP,
            WORLD_GAME_RULE_CHANGE,
            WORLD_DIFFICULTY_CHANGE,
            SERVER_LOAD,
            SERVER_STOP,
            SERVER_PING,
            SERVER_COMMAND,
            SERVER_BROADCAST,
            ENTITY_SPAWN,
            ENTITY_TAME,
            ENTITY_BREED,
            ENTITY_MOUNT,
            ENTITY_HEAL,
            ENTITY_SHOOT,
            ITEM_SPAWN,
            PLAYER_LOGIN,
            PLAYER_BED,
            PLAYER_EFFECT,
            INVENTORY_OPEN,
            INVENTORY_CLOSE,
            INVENTORY_CLICK,
            INVENTORY_SMELT,
            INVENTORY_MOVE_ITEM,
            INVENTORY_TRADE,
            BLOCK_DAMAGE,
            PLAYER_ITEM_DAMAGE,
            PLAYER_SPRINT,
            PLAYER_PICK_ITEM,
            PLAYER_PORTAL,
            RAID_FINISH,
            ENTITY_COMBUST,
            BLOCK_FADE,
        )

    /** The two names that are not broadcast: they are about one plugin's own life. */
    public const val PLUGIN_ENABLE: String = "plugin.enable"
    public const val PLUGIN_DISABLE: String = "plugin.disable"
}
