// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.schemas;
import com.mojang.datafixers.DSL;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.Type;
import com.mojang.datafixers.types.templates.Hook;
import com.mojang.datafixers.types.templates.TypeTemplate;
import com.mojang.datafixers.types.templates.Hook.HookFunction;
import java.util.Map;
import java.util.function.Supplier;
public class V704 extends Schema {
    protected static final Map<String, String> ITEM_TO_BLOCKENTITY = prefixed(
        "furnace=furnace lit_furnace=furnace chest=chest trapped_chest=chest ender_chest=ender_chest jukebox=jukebox dispenser=dispenser dropper=dropper sign=sign mob_spawner=mob_spawner spawner=mob_spawner noteblock=noteblock brewing_stand=brewing_stand enhanting_table=enchanting_table command_block=command_block beacon=beacon skull=skull daylight_detector=daylight_detector hopper=hopper banner=banner flower_pot=flower_pot repeating_command_block=command_block chain_command_block=command_block shulker_box=shulker_box white_shulker_box=shulker_box orange_shulker_box=shulker_box magenta_shulker_box=shulker_box light_blue_shulker_box=shulker_box yellow_shulker_box=shulker_box lime_shulker_box=shulker_box pink_shulker_box=shulker_box gray_shulker_box=shulker_box silver_shulker_box=shulker_box cyan_shulker_box=shulker_box purple_shulker_box=shulker_box blue_shulker_box=shulker_box brown_shulker_box=shulker_box green_shulker_box=shulker_box red_shulker_box=shulker_box black_shulker_box=shulker_box bed=bed light_gray_shulker_box=shulker_box banner=banner white_banner=banner orange_banner=banner magenta_banner=banner light_blue_banner=banner yellow_banner=banner lime_banner=banner pink_banner=banner gray_banner=banner silver_banner=banner light_gray_banner=banner cyan_banner=banner purple_banner=banner blue_banner=banner brown_banner=banner green_banner=banner red_banner=banner black_banner=banner standing_sign=sign wall_sign=sign piston_head=piston daylight_detector_inverted=daylight_detector unpowered_comparator=comparator powered_comparator=comparator wall_banner=banner standing_banner=banner structure_block=structure_block end_portal=end_portal end_gateway=end_gateway sign=sign shield=banner white_bed=bed orange_bed=bed magenta_bed=bed light_blue_bed=bed yellow_bed=bed lime_bed=bed pink_bed=bed gray_bed=bed silver_bed=bed light_gray_bed=bed cyan_bed=bed purple_bed=bed blue_bed=bed brown_bed=bed green_bed=bed red_bed=bed black_bed=bed oak_sign=sign spruce_sign=sign birch_sign=sign jungle_sign=sign acacia_sign=sign dark_oak_sign=sign crimson_sign=sign warped_sign=sign skeleton_skull=skull wither_skeleton_skull=skull zombie_head=skull player_head=skull creeper_head=skull dragon_head=skull barrel=barrel conduit=conduit smoker=smoker blast_furnace=blast_furnace lectern=lectern bell=bell jigsaw=jigsaw campfire=campfire bee_nest=beehive beehive=beehive sculk_sensor=sculk_sensor decorated_pot=decorated_pot crafter=crafter");
    protected static final Hook.HookFunction ADD_NAMES = new Hook.HookFunction() {
        public <T> T apply(com.mojang.serialization.DynamicOps<T> ops, T value) {
            return V99.addNames(new com.mojang.serialization.Dynamic<>(ops, value), V704.ITEM_TO_BLOCKENTITY, V99.ITEM_TO_ENTITY);
        }
    };
    private static Map<String, String> prefixed(String packed) {
        Map<String, String> out = new java.util.HashMap<>();
        org.basaltmc.bukkit.facade.DatafixTables.pairs(packed).forEach((key, value) -> out.put("minecraft:" + key, "minecraft:" + value));
        return java.util.Collections.unmodifiableMap(out);
    }
    public V704(int versionKey, Schema parent) {
        super(versionKey, parent);
    }
    public Type<?> getChoiceType(DSL.TypeReference type, String choiceName) {
        return super.getChoiceType(type, NamespacedSchema.ensureNamespaced(choiceName));
    }
    public Map<String, Supplier<TypeTemplate>> registerBlockEntities(Schema schema) {
        return getParent() == null ? new java.util.HashMap<>() : super.registerBlockEntities(schema);
    }
    public void registerTypes(Schema schema, Map<String, Supplier<TypeTemplate>> entityTypes, Map<String, Supplier<TypeTemplate>> blockEntityTypes) {
    }
}
