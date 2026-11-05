// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import java.util.Set;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.block.ColorCollection;
public class BuiltInLootTables {
    private static final java.util.Map<String, String> kByField = scan();

    private static java.util.Map<String, String> scan() {
        java.util.Map<String, String> map = new java.util.HashMap<>();
        String[] ids = org.basaltmc.bukkit.MiscNatives.lootTables();
        if (ids != null) {
            for (String id : ids) {
                String path = id.contains(":") ? id.substring(id.indexOf(':') + 1) : id;
                String last = path.substring(path.lastIndexOf('/') + 1).toUpperCase(java.util.Locale.ROOT);
                if (path.startsWith("chests/") || !map.containsKey(last)) {
                    map.put(last, path);
                }
                map.putIfAbsent(path.replace('/', '_').toUpperCase(java.util.Locale.ROOT), path);
            }
        }
        return map;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static ResourceKey<LootTable> table(String field) {
        String path = kByField.get(field);
        return (ResourceKey) ResourceKey.create(net.minecraft.core.registries.Registries.LOOT_TABLE,
            net.minecraft.resources.Identifier.withDefaultNamespace(path != null ? path : field.toLowerCase(java.util.Locale.ROOT)));
    }

    public static final ResourceKey<LootTable> SPAWN_BONUS_CHEST = table("SPAWN_BONUS_CHEST");
    public static final ResourceKey<LootTable> END_CITY_TREASURE = table("END_CITY_TREASURE");
    public static final ResourceKey<LootTable> SIMPLE_DUNGEON = table("SIMPLE_DUNGEON");
    public static final ResourceKey<LootTable> VILLAGE_WEAPONSMITH = table("VILLAGE_WEAPONSMITH");
    public static final ResourceKey<LootTable> VILLAGE_TOOLSMITH = table("VILLAGE_TOOLSMITH");
    public static final ResourceKey<LootTable> VILLAGE_ARMORER = table("VILLAGE_ARMORER");
    public static final ResourceKey<LootTable> VILLAGE_CARTOGRAPHER = table("VILLAGE_CARTOGRAPHER");
    public static final ResourceKey<LootTable> VILLAGE_MASON = table("VILLAGE_MASON");
    public static final ResourceKey<LootTable> VILLAGE_SHEPHERD = table("VILLAGE_SHEPHERD");
    public static final ResourceKey<LootTable> VILLAGE_BUTCHER = table("VILLAGE_BUTCHER");
    public static final ResourceKey<LootTable> VILLAGE_FLETCHER = table("VILLAGE_FLETCHER");
    public static final ResourceKey<LootTable> VILLAGE_FISHER = table("VILLAGE_FISHER");
    public static final ResourceKey<LootTable> VILLAGE_TANNERY = table("VILLAGE_TANNERY");
    public static final ResourceKey<LootTable> VILLAGE_TEMPLE = table("VILLAGE_TEMPLE");
    public static final ResourceKey<LootTable> VILLAGE_DESERT_HOUSE = table("VILLAGE_DESERT_HOUSE");
    public static final ResourceKey<LootTable> VILLAGE_PLAINS_HOUSE = table("VILLAGE_PLAINS_HOUSE");
    public static final ResourceKey<LootTable> VILLAGE_TAIGA_HOUSE = table("VILLAGE_TAIGA_HOUSE");
    public static final ResourceKey<LootTable> VILLAGE_SNOWY_HOUSE = table("VILLAGE_SNOWY_HOUSE");
    public static final ResourceKey<LootTable> VILLAGE_SAVANNA_HOUSE = table("VILLAGE_SAVANNA_HOUSE");
    public static final ResourceKey<LootTable> ABANDONED_MINESHAFT = table("ABANDONED_MINESHAFT");
    public static final ResourceKey<LootTable> NETHER_BRIDGE = table("NETHER_BRIDGE");
    public static final ResourceKey<LootTable> STRONGHOLD_LIBRARY = table("STRONGHOLD_LIBRARY");
    public static final ResourceKey<LootTable> STRONGHOLD_CROSSING = table("STRONGHOLD_CROSSING");
    public static final ResourceKey<LootTable> STRONGHOLD_CORRIDOR = table("STRONGHOLD_CORRIDOR");
    public static final ResourceKey<LootTable> DESERT_PYRAMID = table("DESERT_PYRAMID");
    public static final ResourceKey<LootTable> JUNGLE_TEMPLE = table("JUNGLE_TEMPLE");
    public static final ResourceKey<LootTable> JUNGLE_TEMPLE_DISPENSER = table("JUNGLE_TEMPLE_DISPENSER");
    public static final ResourceKey<LootTable> IGLOO_CHEST = table("IGLOO_CHEST");
    public static final ResourceKey<LootTable> WOODLAND_MANSION = table("WOODLAND_MANSION");
    public static final ResourceKey<LootTable> UNDERWATER_RUIN_SMALL = table("UNDERWATER_RUIN_SMALL");
    public static final ResourceKey<LootTable> UNDERWATER_RUIN_BIG = table("UNDERWATER_RUIN_BIG");
    public static final ResourceKey<LootTable> BURIED_TREASURE = table("BURIED_TREASURE");
    public static final ResourceKey<LootTable> SHIPWRECK_MAP = table("SHIPWRECK_MAP");
    public static final ResourceKey<LootTable> SHIPWRECK_SUPPLY = table("SHIPWRECK_SUPPLY");
    public static final ResourceKey<LootTable> SHIPWRECK_TREASURE = table("SHIPWRECK_TREASURE");
    public static final ResourceKey<LootTable> PILLAGER_OUTPOST = table("PILLAGER_OUTPOST");
    public static final ResourceKey<LootTable> BASTION_TREASURE = table("BASTION_TREASURE");
    public static final ResourceKey<LootTable> BASTION_OTHER = table("BASTION_OTHER");
    public static final ResourceKey<LootTable> BASTION_BRIDGE = table("BASTION_BRIDGE");
    public static final ResourceKey<LootTable> BASTION_HOGLIN_STABLE = table("BASTION_HOGLIN_STABLE");
    public static final ResourceKey<LootTable> ANCIENT_CITY = table("ANCIENT_CITY");
    public static final ResourceKey<LootTable> ANCIENT_CITY_ICE_BOX = table("ANCIENT_CITY_ICE_BOX");
    public static final ResourceKey<LootTable> RUINED_PORTAL = table("RUINED_PORTAL");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_REWARD = table("TRIAL_CHAMBERS_REWARD");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_REWARD_COMMON = table("TRIAL_CHAMBERS_REWARD_COMMON");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_REWARD_RARE = table("TRIAL_CHAMBERS_REWARD_RARE");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_REWARD_UNIQUE = table("TRIAL_CHAMBERS_REWARD_UNIQUE");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_REWARD_OMINOUS = table("TRIAL_CHAMBERS_REWARD_OMINOUS");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_REWARD_OMINOUS_COMMON = table("TRIAL_CHAMBERS_REWARD_OMINOUS_COMMON");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_REWARD_OMINOUS_RARE = table("TRIAL_CHAMBERS_REWARD_OMINOUS_RARE");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_REWARD_OMINOUS_UNIQUE = table("TRIAL_CHAMBERS_REWARD_OMINOUS_UNIQUE");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_SUPPLY = table("TRIAL_CHAMBERS_SUPPLY");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_CORRIDOR = table("TRIAL_CHAMBERS_CORRIDOR");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_INTERSECTION = table("TRIAL_CHAMBERS_INTERSECTION");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_INTERSECTION_BARREL = table("TRIAL_CHAMBERS_INTERSECTION_BARREL");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_ENTRANCE = table("TRIAL_CHAMBERS_ENTRANCE");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_CORRIDOR_DISPENSER = table("TRIAL_CHAMBERS_CORRIDOR_DISPENSER");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_CHAMBER_DISPENSER = table("TRIAL_CHAMBERS_CHAMBER_DISPENSER");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_WATER_DISPENSER = table("TRIAL_CHAMBERS_WATER_DISPENSER");
    public static final ResourceKey<LootTable> TRIAL_CHAMBERS_CORRIDOR_POT = table("TRIAL_CHAMBERS_CORRIDOR_POT");
    public static final ResourceKey<LootTable> EQUIPMENT_TRIAL_CHAMBER = table("EQUIPMENT_TRIAL_CHAMBER");
    public static final ResourceKey<LootTable> EQUIPMENT_TRIAL_CHAMBER_RANGED = table("EQUIPMENT_TRIAL_CHAMBER_RANGED");
    public static final ResourceKey<LootTable> EQUIPMENT_TRIAL_CHAMBER_MELEE = table("EQUIPMENT_TRIAL_CHAMBER_MELEE");
    public static final ColorCollection<ResourceKey<LootTable>> SHEEP = null;
    public static final ResourceKey<LootTable> FISHING = table("FISHING");
    public static final ResourceKey<LootTable> FISHING_JUNK = table("FISHING_JUNK");
    public static final ResourceKey<LootTable> FISHING_TREASURE = table("FISHING_TREASURE");
    public static final ResourceKey<LootTable> FISHING_FISH = table("FISHING_FISH");
    public static final ResourceKey<LootTable> CAT_MORNING_GIFT = table("CAT_MORNING_GIFT");
    public static final ResourceKey<LootTable> ARMORER_GIFT = table("ARMORER_GIFT");
    public static final ResourceKey<LootTable> BUTCHER_GIFT = table("BUTCHER_GIFT");
    public static final ResourceKey<LootTable> CARTOGRAPHER_GIFT = table("CARTOGRAPHER_GIFT");
    public static final ResourceKey<LootTable> CLERIC_GIFT = table("CLERIC_GIFT");
    public static final ResourceKey<LootTable> FARMER_GIFT = table("FARMER_GIFT");
    public static final ResourceKey<LootTable> FISHERMAN_GIFT = table("FISHERMAN_GIFT");
    public static final ResourceKey<LootTable> FLETCHER_GIFT = table("FLETCHER_GIFT");
    public static final ResourceKey<LootTable> LEATHERWORKER_GIFT = table("LEATHERWORKER_GIFT");
    public static final ResourceKey<LootTable> LIBRARIAN_GIFT = table("LIBRARIAN_GIFT");
    public static final ResourceKey<LootTable> MASON_GIFT = table("MASON_GIFT");
    public static final ResourceKey<LootTable> SHEPHERD_GIFT = table("SHEPHERD_GIFT");
    public static final ResourceKey<LootTable> TOOLSMITH_GIFT = table("TOOLSMITH_GIFT");
    public static final ResourceKey<LootTable> WEAPONSMITH_GIFT = table("WEAPONSMITH_GIFT");
    public static final ResourceKey<LootTable> UNEMPLOYED_GIFT = table("UNEMPLOYED_GIFT");
    public static final ResourceKey<LootTable> BABY_VILLAGER_GIFT = table("BABY_VILLAGER_GIFT");
    public static final ResourceKey<LootTable> SNIFFER_DIGGING = table("SNIFFER_DIGGING");
    public static final ResourceKey<LootTable> PANDA_SNEEZE = table("PANDA_SNEEZE");
    public static final ResourceKey<LootTable> CHICKEN_LAY = table("CHICKEN_LAY");
    public static final ResourceKey<LootTable> ARMADILLO_SHED = table("ARMADILLO_SHED");
    public static final ResourceKey<LootTable> TURTLE_GROW = table("TURTLE_GROW");
    public static final ResourceKey<LootTable> HARVEST_CAVE_VINE = table("HARVEST_CAVE_VINE");
    public static final ResourceKey<LootTable> HARVEST_SWEET_BERRY_BUSH = table("HARVEST_SWEET_BERRY_BUSH");
    public static final ResourceKey<LootTable> HARVEST_BEEHIVE = table("HARVEST_BEEHIVE");
    public static final ResourceKey<LootTable> CARVE_PUMPKIN = table("CARVE_PUMPKIN");
    public static final ResourceKey<LootTable> TILL_ROOTED_DIRT = table("TILL_ROOTED_DIRT");
    public static final ResourceKey<LootTable> PIGLIN_BARTERING = table("PIGLIN_BARTERING");
    public static final ResourceKey<LootTable> SPAWNER_TRIAL_CHAMBER_KEY = table("SPAWNER_TRIAL_CHAMBER_KEY");
    public static final ResourceKey<LootTable> SPAWNER_TRIAL_CHAMBER_CONSUMABLES = table("SPAWNER_TRIAL_CHAMBER_CONSUMABLES");
    public static final ResourceKey<LootTable> SPAWNER_OMINOUS_TRIAL_CHAMBER_KEY = table("SPAWNER_OMINOUS_TRIAL_CHAMBER_KEY");
    public static final ResourceKey<LootTable> SPAWNER_OMINOUS_TRIAL_CHAMBER_CONSUMABLES = table("SPAWNER_OMINOUS_TRIAL_CHAMBER_CONSUMABLES");
    public static final ResourceKey<LootTable> SPAWNER_TRIAL_ITEMS_TO_DROP_WHEN_OMINOUS = table("SPAWNER_TRIAL_ITEMS_TO_DROP_WHEN_OMINOUS");
    public static final ResourceKey<LootTable> ARMADILLO_BRUSH = table("ARMADILLO_BRUSH");
    public static final ResourceKey<LootTable> BOGGED_SHEAR = table("BOGGED_SHEAR");
    public static final ResourceKey<LootTable> SHEAR_MOOSHROOM = table("SHEAR_MOOSHROOM");
    public static final ResourceKey<LootTable> SHEAR_RED_MOOSHROOM = table("SHEAR_RED_MOOSHROOM");
    public static final ResourceKey<LootTable> SHEAR_BROWN_MOOSHROOM = table("SHEAR_BROWN_MOOSHROOM");
    public static final ResourceKey<LootTable> SHEAR_SNOW_GOLEM = table("SHEAR_SNOW_GOLEM");
    public static final ResourceKey<LootTable> SHEAR_SHEEP = table("SHEAR_SHEEP");
    public static final ColorCollection<ResourceKey<LootTable>> SHEAR_DYED_SHEEP = null;
    public static final ResourceKey<LootTable> CHARGED_CREEPER = table("CHARGED_CREEPER");
    public static final ResourceKey<LootTable> CHARGED_CREEPER_PIGLIN = table("CHARGED_CREEPER_PIGLIN");
    public static final ResourceKey<LootTable> CHARGED_CREEPER_CREEPER = table("CHARGED_CREEPER_CREEPER");
    public static final ResourceKey<LootTable> CHARGED_CREEPER_SKELETON = table("CHARGED_CREEPER_SKELETON");
    public static final ResourceKey<LootTable> CHARGED_CREEPER_WITHER_SKELETON = table("CHARGED_CREEPER_WITHER_SKELETON");
    public static final ResourceKey<LootTable> CHARGED_CREEPER_ZOMBIE = table("CHARGED_CREEPER_ZOMBIE");
    public static final ResourceKey<LootTable> DESERT_WELL_ARCHAEOLOGY = table("DESERT_WELL_ARCHAEOLOGY");
    public static final ResourceKey<LootTable> DESERT_PYRAMID_ARCHAEOLOGY = table("DESERT_PYRAMID_ARCHAEOLOGY");
    public static final ResourceKey<LootTable> TRAIL_RUINS_ARCHAEOLOGY_COMMON = table("TRAIL_RUINS_ARCHAEOLOGY_COMMON");
    public static final ResourceKey<LootTable> TRAIL_RUINS_ARCHAEOLOGY_RARE = table("TRAIL_RUINS_ARCHAEOLOGY_RARE");
    public static final ResourceKey<LootTable> OCEAN_RUIN_WARM_ARCHAEOLOGY = table("OCEAN_RUIN_WARM_ARCHAEOLOGY");
    public static final ResourceKey<LootTable> OCEAN_RUIN_COLD_ARCHAEOLOGY = table("OCEAN_RUIN_COLD_ARCHAEOLOGY");
    public static final ResourceKey<LootTable> ABANDONED_CAMP_BARREL = table("ABANDONED_CAMP_BARREL");
    public static final ResourceKey<LootTable> ABANDONED_CAMP_COMMON_CHEST = table("ABANDONED_CAMP_COMMON_CHEST");
    public static final ResourceKey<LootTable> ABANDONED_CAMP_SECRET_CHEST = table("ABANDONED_CAMP_SECRET_CHEST");
    @SuppressWarnings("unchecked")
    public static Set<ResourceKey<LootTable>> all() {
        Set<ResourceKey<LootTable>> keys = new java.util.LinkedHashSet<>();
        for (java.lang.reflect.Field field : BuiltInLootTables.class.getFields()) {
            if (field.getType() == ResourceKey.class) {
                try {
                    keys.add((ResourceKey<LootTable>) field.get(null));
                } catch (IllegalAccessException ignored) {
                    continue;
                }
            }
        }
        return java.util.Collections.unmodifiableSet(keys);
    }
}
