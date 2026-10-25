#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>

namespace basalt::server::plugins::pluginEventTypes {
const PluginEventType& playerJoin() {
    static const PluginEventType type{
        "player.join", false, {"player_name", "player_uuid", "world", "message", "silent"}, {"message", "silent"}};
    return type;
}

const PluginEventType& playerQuit() {
    static const PluginEventType type{
        "player.quit", false, {"player_name", "player_uuid", "world", "message", "silent"}, {"message", "silent"}};
    return type;
}

const PluginEventType& playerChat() {
    static const PluginEventType type{
        "player.chat", true, {"player_name", "player_uuid", "world", "message"}, {"message"}};
    return type;
}

const PluginEventType& playerCommand() {
    static const PluginEventType type{
        "player.command", true, {"player_name", "player_uuid", "world", "command"}, {"command"}};
    return type;
}

const PluginEventType& playerDeath() {
    static const PluginEventType type{"player.death",
                                      false,
                                      {"player_name", "player_uuid", "world", "x", "y", "z", "cause", "message",
                                       "silent", "drops", "exp", "keep_inventory", "keep_level", "show_messages"},
                                      {"message", "silent", "drops", "exp", "keep_inventory", "keep_level"}};
    return type;
}

const PluginEventType& playerMove() {
    static const PluginEventType type{
        "player.move", true, {"player_name", "player_uuid", "world", "from_x", "from_y", "from_z", "x", "y", "z"}};
    return type;
}

const PluginEventType& playerTeleport() {
    static const PluginEventType type{
        "player.teleport",
        true,
        {"player_name", "player_uuid", "world", "from_x", "from_y", "from_z", "x", "y", "z", "cause"}};
    return type;
}

const PluginEventType& playerWorldChange() {
    static const PluginEventType type{
        "player.world_change", false, {"player_name", "player_uuid", "world", "from_world"}};
    return type;
}

const PluginEventType& playerRespawn() {
    static const PluginEventType type{
        "player.respawn", false, {"player_name", "player_uuid", "world", "x", "y", "z"}, {"x", "y", "z"}};
    return type;
}

const PluginEventType& playerInteract() {
    static const PluginEventType type{
        "player.interact",
        true,
        {"player_name", "player_uuid", "world", "action", "x", "y", "z", "block", "item", "hand", "face"}};
    return type;
}

const PluginEventType& playerInteractEntity() {
    static const PluginEventType type{
        "player.interact_entity",
        true,
        {"player_name", "player_uuid", "world", "entity_id", "entity_type", "action", "hand"}};
    return type;
}

const PluginEventType& playerItemDrop() {
    static const PluginEventType type{
        "player.item_drop", true, {"player_name", "player_uuid", "world", "item", "count"}};
    return type;
}

const PluginEventType& playerItemPickup() {
    static const PluginEventType type{
        "player.item_pickup", true, {"player_name", "player_uuid", "world", "item", "count", "entity_id"}};
    return type;
}

const PluginEventType& playerItemHeld() {
    static const PluginEventType type{
        "player.item_held", false, {"player_name", "player_uuid", "world", "slot", "from_slot"}};
    return type;
}

const PluginEventType& playerItemConsume() {
    static const PluginEventType type{"player.item_consume", true, {"player_name", "player_uuid", "world", "item"}};
    return type;
}

const PluginEventType& playerSwapHands() {
    static const PluginEventType type{
        "player.swap_hands", true, {"player_name", "player_uuid", "world", "main_item", "off_item"}};
    return type;
}

const PluginEventType& playerSneak() {
    static const PluginEventType type{"player.sneak", false, {"player_name", "player_uuid", "world", "sneaking"}};
    return type;
}

const PluginEventType& playerGamemodeChange() {
    static const PluginEventType type{
        "player.gamemode_change", true, {"player_name", "player_uuid", "world", "gamemode", "from_gamemode"}};
    return type;
}

const PluginEventType& playerFoodChange() {
    static const PluginEventType type{
        "player.food_change", true, {"player_name", "player_uuid", "world", "food", "from_food", "item"}, {"food"}};
    return type;
}

const PluginEventType& playerLevelChange() {
    static const PluginEventType type{
        "player.level_change", false, {"player_name", "player_uuid", "world", "level", "from_level"}};
    return type;
}

const PluginEventType& playerCustomClick() {
    static const PluginEventType type{
        "player.custom_click", false, {"player_name", "player_uuid", "world", "id", "payload"}};
    return type;
}

const PluginEventType& playerSignChange() {
    static const PluginEventType type{
        "player.sign_change",
        true,
        {"player_name", "player_uuid", "world", "x", "y", "z", "line1", "line2", "line3", "line4"},
        {"line1", "line2", "line3", "line4"}};
    return type;
}

const PluginEventType& playerBucket() {
    static const PluginEventType type{
        "player.bucket", true, {"player_name", "player_uuid", "world", "x", "y", "z", "item", "filling", "face"}};
    return type;
}

const PluginEventType& blockBreak() {
    static const PluginEventType type{"block.break",
                                      true,
                                      {"player_name", "player_uuid", "world", "x", "y", "z", "block", "drop_items"},
                                      {"drop_items"}};
    return type;
}

const PluginEventType& blockPlace() {
    static const PluginEventType type{
        "block.place", true, {"player_name", "player_uuid", "world", "x", "y", "z", "block"}};
    return type;
}

const PluginEventType& entityDamage() {
    static const PluginEventType type{
        "entity.damage",
        true,
        {"entity_id", "entity_type", "world", "cause", "amount", "attacker_name", "attacker_id", "direct_id"},
        {"amount"}};
    return type;
}

const PluginEventType& entityDeath() {
    static const PluginEventType type{
        "entity.death", false, {"entity_id", "entity_type", "world", "x", "y", "z", "killer_name", "killer_id"}};
    return type;
}

const PluginEventType& serverTick() {
    static const PluginEventType type{"server.tick", false, {"tick"}};
    return type;
}

const PluginEventType& blockExplode() {
    static const PluginEventType type{"block.explode", true, {"world", "x", "y", "z", "power", "cause", "entity_id"}};
    return type;
}

const PluginEventType& blockIgnite() {
    static const PluginEventType type{"block.ignite", true, {"world", "x", "y", "z", "cause", "player_name"}};
    return type;
}

const PluginEventType& blockGrow() {
    static const PluginEventType type{"block.grow", true, {"world", "x", "y", "z", "block", "to_block", "cause"}};
    return type;
}

const PluginEventType& blockDispense() {
    static const PluginEventType type{"block.dispense", true, {"world", "x", "y", "z", "item", "face"}};
    return type;
}

const PluginEventType& blockPiston() {
    static const PluginEventType type{
        "block.piston", true, {"world", "x", "y", "z", "extending", "direction", "moved"}};
    return type;
}

const PluginEventType& worldLoad() {
    static const PluginEventType type{"world.load", false, {"world"}};
    return type;
}

const PluginEventType& worldSave() {
    static const PluginEventType type{"world.save", false, {"world"}};
    return type;
}

const PluginEventType& worldChunkLoad() {
    static const PluginEventType type{"world.chunk_load", false, {"world", "chunk_x", "chunk_z", "generated"}};
    return type;
}

const PluginEventType& worldChunkUnload() {
    static const PluginEventType type{"world.chunk_unload", false, {"world", "chunk_x", "chunk_z"}};
    return type;
}

const PluginEventType& worldWeatherChange() {
    static const PluginEventType type{
        "world.weather_change", true, {"world", "raining", "thundering", "from_raining", "from_thundering"}};
    return type;
}

const PluginEventType& worldLightning() {
    static const PluginEventType type{"world.lightning", true, {"world", "x", "y", "z"}};
    return type;
}

const PluginEventType& worldTimeSkip() {
    static const PluginEventType type{"world.time_skip", true, {"world", "time", "from_time", "cause"}};
    return type;
}

const PluginEventType& worldGameRuleChange() {
    static const PluginEventType type{
        "world.game_rule_change", true, {"world", "rule", "value", "from_value", "player_name"}};
    return type;
}

const PluginEventType& worldDifficultyChange() {
    static const PluginEventType type{
        "world.difficulty_change", true, {"world", "difficulty", "from_difficulty", "player_name"}};
    return type;
}

const PluginEventType& serverLoad() {
    static const PluginEventType type{"server.load", false, {"worlds", "plugins"}};
    return type;
}

const PluginEventType& serverStop() {
    static const PluginEventType type{"server.stop", false, {"players"}};
    return type;
}

const PluginEventType& serverPing() {
    static const PluginEventType type{
        "server.ping", false, {"address", "motd", "online", "max_players"}, {"motd", "max_players"}};
    return type;
}

const PluginEventType& serverCommand() {
    static const PluginEventType type{"server.command", true, {"source", "command"}, {"command"}};
    return type;
}

const PluginEventType& serverBroadcast() {
    static const PluginEventType type{"server.broadcast", true, {"message"}, {"message"}};
    return type;
}

const PluginEventType& entitySpawn() {
    static const PluginEventType type{"entity.spawn", true, {"entity_type", "world", "x", "y", "z", "reason"}};
    return type;
}

const PluginEventType& entityTame() {
    static const PluginEventType type{
        "entity.tame", true, {"entity_id", "entity_type", "world", "player_name", "player_uuid"}};
    return type;
}

const PluginEventType& entityBreed() {
    static const PluginEventType type{"entity.breed",
                                      true,
                                      {"entity_id", "entity_type", "world", "partner_id", "breeder", "experience"},
                                      {"experience"}};
    return type;
}

const PluginEventType& entityMount() {
    static const PluginEventType type{
        "entity.mount", true, {"entity_id", "entity_type", "world", "player_name", "player_uuid", "mounting"}};
    return type;
}

const PluginEventType& entityHeal() {
    static const PluginEventType type{
        "entity.heal", true, {"entity_id", "entity_type", "world", "amount", "cause", "fast_regen"}, {"amount"}};
    return type;
}

const PluginEventType& entityShoot() {
    static const PluginEventType type{
        "entity.shoot", true, {"entity_id", "entity_type", "world", "projectile", "power"}};
    return type;
}

const PluginEventType& itemSpawn() {
    static const PluginEventType type{
        "item.spawn", true, {"world", "x", "y", "z", "entity_id", "item", "count", "cause"}};
    return type;
}

const PluginEventType& playerLogin() {
    static const PluginEventType type{
        "player.login", true, {"player_name", "player_uuid", "address", "message"}, {"message"}};
    return type;
}

const PluginEventType& playerBed() {
    static const PluginEventType type{
        "player.bed", true, {"player_name", "player_uuid", "world", "x", "y", "z", "entering"}};
    return type;
}

const PluginEventType& playerEffect() {
    static const PluginEventType type{
        "player.effect", true, {"player_name", "player_uuid", "world", "effect", "amplifier", "duration"}};
    return type;
}

const PluginEventType& inventoryOpen() {
    static const PluginEventType type{
        "inventory.open", true, {"player_name", "player_uuid", "world", "kind", "x", "y", "z"}};
    return type;
}

const PluginEventType& inventoryClose() {
    static const PluginEventType type{
        "inventory.close", false, {"player_name", "player_uuid", "world", "kind", "reason"}};
    return type;
}

const PluginEventType& inventoryDrag() {
    static const PluginEventType type{"inventory.drag", true, {"player_name", "player_uuid", "world", "type", "slots"}};
    return type;
}

const PluginEventType& inventoryPrepare() {
    static const PluginEventType type{
        "inventory.prepare",
        false,
        {"player_name", "player_uuid", "world", "kind", "recipe", "result", "name", "cost"},
        {"result", "cost"}};
    return type;
}

const PluginEventType& inventoryTake() {
    static const PluginEventType type{
        "inventory.take", true, {"player_name", "player_uuid", "world", "kind", "recipe", "result", "shift"}};
    return type;
}

const PluginEventType& inventoryEnchantPrepare() {
    static const PluginEventType type{
        "inventory.enchant_prepare",
        true,
        {"player_name", "player_uuid", "world", "x", "y", "z", "item", "bookshelves", "offers"},
        {"offers"}};
    return type;
}

const PluginEventType& inventoryEnchant() {
    static const PluginEventType type{
        "inventory.enchant",
        true,
        {"player_name", "player_uuid", "world", "x", "y", "z", "item", "button", "cost", "enchants"},
        {"cost", "enchants"}};
    return type;
}

const PluginEventType& furnaceBurn() {
    static const PluginEventType type{
        "furnace.burn", true, {"world", "x", "y", "z", "cooker", "fuel", "burn_time"}, {"burn_time"}};
    return type;
}

const PluginEventType& furnaceStart() {
    static const PluginEventType type{
        "furnace.start", false, {"world", "x", "y", "z", "cooker", "source", "total", "recipe"}};
    return type;
}

const PluginEventType& inventoryChanged() {
    static const PluginEventType type{"inventory.changed", false, {"player_name", "player_uuid", "world"}};
    return type;
}

const PluginEventType& inventoryClick() {
    static const PluginEventType type{
        "inventory.click", true, {"player_name", "player_uuid", "world", "slot", "button", "mode", "item"}};
    return type;
}

const PluginEventType& inventorySmelt() {
    static const PluginEventType type{
        "inventory.smelt",
        true,
        {"world", "x", "y", "z", "cooker", "item", "result", "source", "output", "recipe"},
        {"output"}};
    return type;
}

const PluginEventType& inventoryMoveItem() {
    static const PluginEventType type{"inventory.move_item", true, {"world", "x", "y", "z", "kind"}};
    return type;
}

const PluginEventType& inventoryTrade() {
    static const PluginEventType type{
        "inventory.trade", true, {"player_name", "player_uuid", "world", "entity_id", "index", "result"}};
    return type;
}

const PluginEventType& blockDamage() {
    static const PluginEventType type{
        "block.damage", true, {"player_name", "player_uuid", "world", "x", "y", "z", "block", "face", "insta_break"}};
    return type;
}

const PluginEventType& playerItemDamage() {
    static const PluginEventType type{
        "player.item_damage", true, {"player_name", "player_uuid", "world", "item", "damage", "broken"}};
    return type;
}

const PluginEventType& playerSprint() {
    static const PluginEventType type{"player.sprint", false, {"player_name", "player_uuid", "world", "sprinting"}};
    return type;
}

const PluginEventType& playerPickItem() {
    static const PluginEventType type{
        "player.pick_item", true, {"player_name", "player_uuid", "world", "x", "y", "z", "item"}};
    return type;
}

const PluginEventType& playerPortal() {
    static const PluginEventType type{
        "player.portal", true, {"player_name", "player_uuid", "world", "to_world", "x", "y", "z", "kind"}};
    return type;
}

const PluginEventType& raidFinish() {
    static const PluginEventType type{"raid.finish", false, {"world", "x", "y", "z", "waves", "won"}};
    return type;
}

const PluginEventType& entityDrops() {
    static const PluginEventType type{"entity.drops",
                                      false,
                                      {"entity_id", "entity_type", "world", "x", "y", "z", "drops", "exp", "killer_id"},
                                      {"drops", "exp"}};
    return type;
}

const PluginEventType& entityLaunch() {
    static const PluginEventType type{
        "entity.launch", true, {"entity_id", "entity_type", "world", "x", "y", "z", "shooter_id"}};
    return type;
}

const PluginEventType& vehicleDamage() {
    static const PluginEventType type{
        "vehicle.damage", true, {"entity_id", "entity_type", "world", "amount", "attacker_id"}, {"amount"}};
    return type;
}

const PluginEventType& vehicleDestroy() {
    static const PluginEventType type{"vehicle.destroy", true, {"entity_id", "entity_type", "world", "attacker_id"}};
    return type;
}

const PluginEventType& entityResurrect() {
    static const PluginEventType type{"entity.resurrect", true, {"entity_id", "entity_type", "world", "hand"}};
    return type;
}

const PluginEventType& entityAdd() {
    static const PluginEventType type{
        "entity.add", false, {"entity_id", "entity_type", "world", "x", "y", "z", "reason"}};
    return type;
}

const PluginEventType& entityTarget() {
    static const PluginEventType type{"entity.target",
                                      true,
                                      {"entity_id", "entity_type", "world", "target_id", "previous_id", "reason"},
                                      {"target_id"}};
    return type;
}

const PluginEventType& entityGoal() {
    static const PluginEventType type{
        "entity.goal", false, {"entity_id", "entity_type", "world", "key", "phase", "result"}, {"result"}};
    return type;
}

const PluginEventType& entityPotionEffect() {
    static const PluginEventType type{
        "entity.potion_effect",
        true,
        {"entity_id", "entity_type", "world", "effect", "amplifier", "duration", "action", "cause"}};
    return type;
}

const PluginEventType& entityCombust() {
    static const PluginEventType type{"entity.combust", true, {"entity_id", "entity_type", "world", "ticks", "cause"}};
    return type;
}

const PluginEventType& blockFade() {
    static const PluginEventType type{"block.fade", true, {"world", "x", "y", "z", "block", "to_block", "cause"}};
    return type;
}

const PluginEventType& playerKick() {
    static const PluginEventType type{
        "player.kick", true, {"player_name", "player_uuid", "world", "reason", "cause"}, {"reason"}};
    return type;
}

const PluginEventType& playerToggleFlight() {
    static const PluginEventType type{"player.toggle_flight", true, {"player_name", "player_uuid", "world", "flying"}};
    return type;
}

const PluginEventType& playerAnimation() {
    static const PluginEventType type{"player.animation", true, {"player_name", "player_uuid", "world", "hand"}};
    return type;
}

const PluginEventType& playerJump() {
    static const PluginEventType type{
        "player.jump", true, {"player_name", "player_uuid", "world", "from_x", "from_y", "from_z", "x", "y", "z"}};
    return type;
}

const PluginEventType& playerEditBook() {
    static const PluginEventType type{"player.edit_book",
                                      true,
                                      {"player_name", "player_uuid", "world", "slot", "signing", "title", "pages"},
                                      {"title", "pages", "signing"}};
    return type;
}

const PluginEventType& playerExpChange() {
    static const PluginEventType type{
        "player.exp_change", false, {"player_name", "player_uuid", "world", "amount", "source_id"}, {"amount"}};
    return type;
}

const PluginEventType& playerArmorChange() {
    static const PluginEventType type{
        "player.armor_change", false, {"player_name", "player_uuid", "world", "slot", "old_item", "new_item"}};
    return type;
}

const PluginEventType& playerAdvancement() {
    static const PluginEventType type{
        "player.advancement", false, {"player_name", "player_uuid", "world", "advancement"}};
    return type;
}

const PluginEventType& playerResourcePack() {
    static const PluginEventType type{
        "player.resource_pack", false, {"player_name", "player_uuid", "world", "pack_id", "status"}};
    return type;
}

const PluginEventType& playerClientOptions() {
    static const PluginEventType type{
        "player.client_options",
        false,
        {"player_name", "player_uuid", "world", "locale", "view_distance", "chat_colors", "main_hand", "skin_parts"}};
    return type;
}

const PluginEventType& playerFish() {
    static const PluginEventType type{
        "player.fish", true, {"player_name", "player_uuid", "world", "state", "hook_id", "caught_id", "exp"}, {"exp"}};
    return type;
}

const PluginEventType& playerTabComplete() {
    static const PluginEventType type{
        "player.tab_complete", true, {"player_name", "player_uuid", "world", "buffer", "completions"}, {"completions"}};
    return type;
}

const PluginEventType& blockBurn() {
    static const PluginEventType type{
        "block.burn", true, {"world", "x", "y", "z", "block", "igniter_x", "igniter_y", "igniter_z"}};
    return type;
}

const PluginEventType& blockFertilize() {
    static const PluginEventType type{
        "block.fertilize", true, {"player_name", "player_uuid", "world", "x", "y", "z", "block"}};
    return type;
}

const PluginEventType& blockFromTo() {
    static const PluginEventType type{
        "block.from_to", true, {"world", "x", "y", "z", "block", "to_x", "to_y", "to_z", "face"}};
    return type;
}

const PluginEventType& blockPhysics() {
    static const PluginEventType type{"block.physics", true, {"world", "x", "y", "z", "block", "source_block"}};
    return type;
}

const PluginEventType& worldStructureGrow() {
    static const PluginEventType type{"world.structure_grow", true, {"world", "x", "y", "z", "species", "player_name"}};
    return type;
}

const PluginEventType& worldSpawnChange() {
    static const PluginEventType type{
        "world.spawn_change", false, {"world", "x", "y", "z", "from_x", "from_y", "from_z"}};
    return type;
}

const PluginEventType& hangingPlace() {
    static const PluginEventType type{
        "hanging.place", true, {"world", "x", "y", "z", "entity_type", "face", "player_name", "player_uuid"}};
    return type;
}

const PluginEventType& hangingBreak() {
    static const PluginEventType type{
        "hanging.break", true, {"entity_id", "entity_type", "world", "x", "y", "z", "cause", "remover_id"}};
    return type;
}

const PluginEventType& serverTickEnd() {
    static const PluginEventType type{"server.tick_end", false, {"tick", "duration_ms", "remaining_ms"}};
    return type;
}

const PluginEventType& projectileHit() {
    static const PluginEventType type{
        "projectile.hit", true, {"entity_id", "entity_type", "world", "hit_id", "x", "y", "z"}};
    return type;
}

const PluginEventType& playerInteractAir() {
    static const PluginEventType type{
        "player.interact_air", true, {"player_name", "player_uuid", "world", "item", "hand"}};
    return type;
}

const PluginEventType& blockDrops() {
    static const PluginEventType type{
        "block.drops", true, {"player_name", "player_uuid", "world", "x", "y", "z", "block", "drops"}, {"drops"}};
    return type;
}

const std::vector<const PluginEventType*>& all() {
    static const std::vector<const PluginEventType*> every = [] {
        std::vector<const PluginEventType*> built{&playerJoin(),
                                                  &playerQuit(),
                                                  &playerChat(),
                                                  &playerCommand(),
                                                  &playerDeath(),
                                                  &playerMove(),
                                                  &playerTeleport(),
                                                  &playerWorldChange(),
                                                  &playerRespawn(),
                                                  &playerInteract(),
                                                  &playerInteractEntity(),
                                                  &playerItemDrop(),
                                                  &playerItemPickup(),
                                                  &playerItemHeld(),
                                                  &playerItemConsume(),
                                                  &playerSwapHands(),
                                                  &playerSneak(),
                                                  &playerGamemodeChange(),
                                                  &playerFoodChange(),
                                                  &playerLevelChange(),
                                                  &playerSignChange(),
                                                  &playerBucket(),
                                                  &blockBreak(),
                                                  &blockPlace(),
                                                  &entityDamage(),
                                                  &entityDeath(),
                                                  &serverTick(),
                                                  &blockExplode(),
                                                  &blockIgnite(),
                                                  &blockGrow(),
                                                  &blockDispense(),
                                                  &blockPiston(),
                                                  &worldLoad(),
                                                  &worldSave(),
                                                  &worldChunkLoad(),
                                                  &worldChunkUnload(),
                                                  &worldWeatherChange(),
                                                  &worldLightning(),
                                                  &worldTimeSkip(),
                                                  &worldGameRuleChange(),
                                                  &worldDifficultyChange(),
                                                  &serverLoad(),
                                                  &serverStop(),
                                                  &serverPing(),
                                                  &serverCommand(),
                                                  &serverBroadcast(),
                                                  &entitySpawn(),
                                                  &entityTame(),
                                                  &entityBreed(),
                                                  &entityMount(),
                                                  &entityHeal(),
                                                  &entityShoot(),
                                                  &itemSpawn(),
                                                  &playerLogin(),
                                                  &playerBed(),
                                                  &playerEffect(),
                                                  &inventoryOpen(),
                                                  &inventoryClose(),
                                                  &inventoryClick(),
                                                  &inventorySmelt(),
                                                  &inventoryMoveItem(),
                                                  &inventoryTrade(),
                                                  &blockDamage(),
                                                  &playerItemDamage(),
                                                  &playerSprint(),
                                                  &playerPickItem(),
                                                  &playerPortal(),
                                                  &raidFinish(),
                                                  &entityCombust(),
                                                  &entityTarget(),
                                                  &entityAdd(),
                                                  &entityResurrect(),
                                                  &entityLaunch(),
                                                  &vehicleDamage(),
                                                  &vehicleDestroy(),
                                                  &entityDrops(),
                                                  &entityPotionEffect(),
                                                  &blockFade(),
                                                  &playerCustomClick(),
                                                  &inventoryDrag(),
                                                  &inventoryChanged(),
                                                  &inventoryPrepare(),
                                                  &inventoryTake(),
                                                  &inventoryEnchantPrepare(),
                                                  &inventoryEnchant(),
                                                  &furnaceBurn(),
                                                  &furnaceStart(),
                                                  &playerKick(),
                                                  &playerToggleFlight(),
                                                  &playerAnimation(),
                                                  &playerJump(),
                                                  &playerEditBook(),
                                                  &playerExpChange(),
                                                  &playerArmorChange(),
                                                  &playerAdvancement(),
                                                  &playerResourcePack(),
                                                  &playerClientOptions(),
                                                  &playerFish(),
                                                  &playerTabComplete(),
                                                  &blockBurn(),
                                                  &blockFertilize(),
                                                  &blockFromTo(),
                                                  &blockPhysics(),
                                                  &worldStructureGrow(),
                                                  &worldSpawnChange(),
                                                  &hangingPlace(),
                                                  &hangingBreak(),
                                                  &serverTickEnd(),
                                                  &projectileHit(),
                                                  &playerInteractAir(),
                                                  &blockDrops()};
        built.insert(built.end(), hookEvents().begin(), hookEvents().end());
        built.push_back(&entityGoal());
        return built;
    }();
    return every;
}
} // namespace basalt::server::plugins::pluginEventTypes
