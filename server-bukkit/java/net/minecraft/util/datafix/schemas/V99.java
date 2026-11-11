// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.schemas;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.templates.Hook;
import com.mojang.datafixers.types.templates.TypeTemplate;
import com.mojang.datafixers.types.templates.Hook.HookFunction;
import com.mojang.serialization.Dynamic;
import java.util.Map;
import java.util.function.Supplier;
public class V99 extends Schema {
    public static final Map<String, String> ITEM_TO_ENTITY = Map.of("minecraft:armor_stand", "ArmorStand", "minecraft:painting", "Painting");
    private static final Map<String, String> kItemToBlockEntity = org.basaltmc.bukkit.facade.DatafixTables.pairs(
        "minecraft:furnace=Furnace minecraft:lit_furnace=Furnace minecraft:chest=Chest minecraft:trapped_chest=Chest minecraft:ender_chest=EnderChest minecraft:jukebox=RecordPlayer minecraft:dispenser=Trap minecraft:dropper=Dropper minecraft:sign=Sign minecraft:mob_spawner=MobSpawner minecraft:noteblock=Music minecraft:brewing_stand=Cauldron minecraft:enhanting_table=EnchantTable minecraft:command_block=CommandBlock minecraft:beacon=Beacon minecraft:skull=Skull minecraft:daylight_detector=DLDetector minecraft:hopper=Hopper minecraft:banner=Banner minecraft:flower_pot=FlowerPot minecraft:repeating_command_block=CommandBlock minecraft:chain_command_block=CommandBlock minecraft:standing_sign=Sign minecraft:wall_sign=Sign minecraft:piston_head=Piston minecraft:daylight_detector_inverted=DLDetector minecraft:unpowered_comparator=Comparator minecraft:powered_comparator=Comparator minecraft:wall_banner=Banner minecraft:standing_banner=Banner minecraft:structure_block=Structure minecraft:end_portal=Airportal minecraft:end_gateway=EndGateway minecraft:shield=Banner");
    protected static final Hook.HookFunction ADD_NAMES = new Hook.HookFunction() {
        public <T> T apply(com.mojang.serialization.DynamicOps<T> ops, T value) {
            return addNames(new Dynamic<>(ops, value), kItemToBlockEntity, ITEM_TO_ENTITY);
        }
    };
    public V99(int versionKey, Schema parent) {
        super(versionKey, parent);
    }
    protected static void registerThrowableProjectile(Schema schema, Map<String, Supplier<TypeTemplate>> map, String name) {
        schema.register(map, name, (java.util.function.Supplier<TypeTemplate>) com.mojang.datafixers.DSL::remainder);
    }
    protected static void registerMinecart(Schema schema, Map<String, Supplier<TypeTemplate>> map, String name) {
        schema.register(map, name, (java.util.function.Supplier<TypeTemplate>) com.mojang.datafixers.DSL::remainder);
    }
    protected static void registerInventory(Schema schema, Map<String, Supplier<TypeTemplate>> map, String name) {
        schema.register(map, name, (java.util.function.Supplier<TypeTemplate>) com.mojang.datafixers.DSL::remainder);
    }
    public Map<String, Supplier<TypeTemplate>> registerEntities(Schema schema) {
        return getParent() == null ? new java.util.HashMap<>() : super.registerEntities(schema);
    }
    public Map<String, Supplier<TypeTemplate>> registerBlockEntities(Schema schema) {
        return getParent() == null ? new java.util.HashMap<>() : super.registerBlockEntities(schema);
    }
    public static TypeTemplate sign(Schema schema) {
        return com.mojang.datafixers.DSL.remainder();
    }
    public void registerTypes(Schema schema, Map<String, Supplier<TypeTemplate>> entityTypes, Map<String, Supplier<TypeTemplate>> blockEntityTypes) {
    }
    public static TypeTemplate itemStackTag(Schema schema) {
        return com.mojang.datafixers.DSL.remainder();
    }
    protected static <T> T addNames(Dynamic<T> input, Map<String, String> itemToBlockEntityMap, Map<String, String> itemToEntityMap) {
        return input.update("tag", tag -> tag.update("BlockEntityTag", blockEntity -> {
            String itemId = input.get("id").asString().result().map(NamespacedSchema::ensureNamespaced).orElse("minecraft:air");
            String expected = "minecraft:air".equals(itemId) ? null : itemToBlockEntityMap.get(itemId);
            return expected == null ? blockEntity : blockEntity.set("id", input.createString(expected));
        }).update("EntityTag", entity -> {
            if (entity.get("id").result().isPresent()) {
                return entity;
            }
            String expected = itemToEntityMap.get(NamespacedSchema.ensureNamespaced(input.get("id").asString("")));
            return expected == null ? entity : entity.set("id", input.createString(expected));
        })).getValue();
    }
}
