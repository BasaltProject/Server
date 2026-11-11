// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import java.util.Map;
public class EntityTheRenameningFix extends SimplestEntityRenameFix {
    public static final Map<String, String> RENAMED_IDS = org.basaltmc.bukkit.facade.DatafixTables.pairs("minecraft:commandblock_minecart=minecraft:command_block_minecart minecraft:ender_crystal=minecraft:end_crystal minecraft:snowman=minecraft:snow_golem minecraft:evocation_illager=minecraft:evoker minecraft:evocation_fangs=minecraft:evoker_fangs minecraft:illusion_illager=minecraft:illusioner minecraft:vindication_illager=minecraft:vindicator minecraft:villager_golem=minecraft:iron_golem minecraft:xp_orb=minecraft:experience_orb minecraft:xp_bottle=minecraft:experience_bottle minecraft:eye_of_ender_signal=minecraft:eye_of_ender minecraft:fireworks_rocket=minecraft:firework_rocket");
    public static final Map<String, String> RENAMED_BLOCKS = org.basaltmc.bukkit.facade.DatafixTables.pairs("minecraft:portal=minecraft:nether_portal minecraft:oak_bark=minecraft:oak_wood minecraft:spruce_bark=minecraft:spruce_wood minecraft:birch_bark=minecraft:birch_wood minecraft:jungle_bark=minecraft:jungle_wood minecraft:acacia_bark=minecraft:acacia_wood minecraft:dark_oak_bark=minecraft:dark_oak_wood minecraft:stripped_oak_bark=minecraft:stripped_oak_wood minecraft:stripped_spruce_bark=minecraft:stripped_spruce_wood minecraft:stripped_birch_bark=minecraft:stripped_birch_wood minecraft:stripped_jungle_bark=minecraft:stripped_jungle_wood minecraft:stripped_acacia_bark=minecraft:stripped_acacia_wood minecraft:stripped_dark_oak_bark=minecraft:stripped_dark_oak_wood minecraft:mob_spawner=minecraft:spawner");
    public static final Map<String, String> RENAMED_ITEMS = items();
    private static Map<String, String> items() {
        Map<String, String> out = new java.util.LinkedHashMap<>(RENAMED_BLOCKS);
        out.putAll(org.basaltmc.bukkit.facade.DatafixTables.pairs("minecraft:clownfish=minecraft:tropical_fish minecraft:chorus_fruit_popped=minecraft:popped_chorus_fruit minecraft:evocation_illager_spawn_egg=minecraft:evoker_spawn_egg minecraft:vindication_illager_spawn_egg=minecraft:vindicator_spawn_egg"));
        return java.util.Collections.unmodifiableMap(out);
    }
    public EntityTheRenameningFix(Schema schema, boolean changesType) {
        super("EntityTheRenameningBlock", schema, changesType);
    }
    protected String rename(String name) {
        String id = name.startsWith("minecraft:bred_") ? "minecraft:" + name.substring(15) : name;
        return RENAMED_IDS.getOrDefault(id, id);
    }
}
