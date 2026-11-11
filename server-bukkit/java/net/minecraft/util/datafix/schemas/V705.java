// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.schemas;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.templates.Hook;
import com.mojang.datafixers.types.templates.TypeTemplate;
import com.mojang.datafixers.types.templates.Hook.HookFunction;
import java.util.Map;
import java.util.function.Supplier;
public class V705 extends NamespacedSchema {
    private static final Map<String, String> kItemToEntity = prefixed(
        "armor_stand=armor_stand painting=painting armadillo_spawn_egg=armadillo allay_spawn_egg=allay axolotl_spawn_egg=axolotl bat_spawn_egg=bat bee_spawn_egg=bee blaze_spawn_egg=blaze bogged_spawn_egg=bogged breeze_spawn_egg=breeze cat_spawn_egg=cat camel_spawn_egg=camel cave_spider_spawn_egg=cave_spider chicken_spawn_egg=chicken cod_spawn_egg=cod cow_spawn_egg=cow creeper_spawn_egg=creeper dolphin_spawn_egg=dolphin donkey_spawn_egg=donkey drowned_spawn_egg=drowned elder_guardian_spawn_egg=elder_guardian ender_dragon_spawn_egg=ender_dragon enderman_spawn_egg=enderman endermite_spawn_egg=endermite evoker_spawn_egg=evoker fox_spawn_egg=fox frog_spawn_egg=frog ghast_spawn_egg=ghast glow_squid_spawn_egg=glow_squid goat_spawn_egg=goat guardian_spawn_egg=guardian hoglin_spawn_egg=hoglin horse_spawn_egg=horse husk_spawn_egg=husk iron_golem_spawn_egg=iron_golem llama_spawn_egg=llama magma_cube_spawn_egg=magma_cube mooshroom_spawn_egg=mooshroom mule_spawn_egg=mule ocelot_spawn_egg=ocelot panda_spawn_egg=panda parrot_spawn_egg=parrot phantom_spawn_egg=phantom pig_spawn_egg=pig piglin_spawn_egg=piglin piglin_brute_spawn_egg=piglin_brute pillager_spawn_egg=pillager polar_bear_spawn_egg=polar_bear pufferfish_spawn_egg=pufferfish rabbit_spawn_egg=rabbit ravager_spawn_egg=ravager salmon_spawn_egg=salmon sheep_spawn_egg=sheep shulker_spawn_egg=shulker silverfish_spawn_egg=silverfish skeleton_spawn_egg=skeleton skeleton_horse_spawn_egg=skeleton_horse slime_spawn_egg=slime sniffer_spawn_egg=sniffer snow_golem_spawn_egg=snow_golem spider_spawn_egg=spider squid_spawn_egg=squid stray_spawn_egg=stray strider_spawn_egg=strider tadpole_spawn_egg=tadpole trader_llama_spawn_egg=trader_llama tropical_fish_spawn_egg=tropical_fish turtle_spawn_egg=turtle vex_spawn_egg=vex villager_spawn_egg=villager vindicator_spawn_egg=vindicator wandering_trader_spawn_egg=wandering_trader warden_spawn_egg=warden witch_spawn_egg=witch wither_spawn_egg=wither wither_skeleton_spawn_egg=wither_skeleton wolf_spawn_egg=wolf zoglin_spawn_egg=zoglin zombie_spawn_egg=zombie zombie_horse_spawn_egg=zombie_horse zombie_villager_spawn_egg=zombie_villager zombified_piglin_spawn_egg=zombified_piglin item_frame=item_frame boat=oak_boat oak_boat=oak_boat oak_chest_boat=oak_chest_boat spruce_boat=spruce_boat spruce_chest_boat=spruce_chest_boat birch_boat=birch_boat birch_chest_boat=birch_chest_boat jungle_boat=jungle_boat jungle_chest_boat=jungle_chest_boat acacia_boat=acacia_boat acacia_chest_boat=acacia_chest_boat cherry_boat=cherry_boat cherry_chest_boat=cherry_chest_boat dark_oak_boat=dark_oak_boat dark_oak_chest_boat=dark_oak_chest_boat mangrove_boat=mangrove_boat mangrove_chest_boat=mangrove_chest_boat bamboo_raft=bamboo_raft bamboo_chest_raft=bamboo_chest_raft minecart=minecart chest_minecart=chest_minecart furnace_minecart=furnace_minecart tnt_minecart=tnt_minecart hopper_minecart=hopper_minecart");
    protected static final Hook.HookFunction ADD_NAMES = new Hook.HookFunction() {
        public <T> T apply(com.mojang.serialization.DynamicOps<T> ops, T value) {
            return V99.addNames(new com.mojang.serialization.Dynamic<>(ops, value), V704.ITEM_TO_BLOCKENTITY, kItemToEntity);
        }
    };
    private static Map<String, String> prefixed(String packed) {
        Map<String, String> out = new java.util.HashMap<>();
        org.basaltmc.bukkit.facade.DatafixTables.pairs(packed).forEach((key, value) -> out.put("minecraft:" + key, "minecraft:" + value));
        return java.util.Collections.unmodifiableMap(out);
    }
    public V705(int versionKey, Schema parent) {
        super(versionKey, parent);
    }
    protected static void registerMob(Schema schema, Map<String, Supplier<TypeTemplate>> map, String name) {
        schema.register(map, name, (java.util.function.Supplier<TypeTemplate>) com.mojang.datafixers.DSL::remainder);
    }
    protected static void registerThrowableProjectile(Schema schema, Map<String, Supplier<TypeTemplate>> map, String name) {
        schema.register(map, name, (java.util.function.Supplier<TypeTemplate>) com.mojang.datafixers.DSL::remainder);
    }
    public Map<String, Supplier<TypeTemplate>> registerEntities(Schema schema) {
        return getParent() == null ? new java.util.HashMap<>() : super.registerEntities(schema);
    }
    public void registerTypes(Schema schema, Map<String, Supplier<TypeTemplate>> entityTypes, Map<String, Supplier<TypeTemplate>> blockEntityTypes) {
    }
}
