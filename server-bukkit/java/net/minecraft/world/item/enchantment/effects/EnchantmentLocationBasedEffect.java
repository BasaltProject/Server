// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Registry;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.phys.Vec3;
public interface EnchantmentLocationBasedEffect {
    Codec<EnchantmentLocationBasedEffect> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.enchantment.effects.EnchantmentLocationBasedEffect.class, "CODEC");
    static MapCodec<? extends EnchantmentLocationBasedEffect> bootstrap(Registry<MapCodec<? extends EnchantmentLocationBasedEffect>> registry) {
        Object[][] entries = {
            {"all_of", AllOf.LocationBasedEffects.CODEC},
            {"attribute", EnchantmentAttributeEffect.MAP_CODEC},
            {"apply_mob_effect", ApplyMobEffect.CODEC},
            {"change_item_damage", ChangeItemDamage.CODEC},
            {"damage_entity", DamageEntity.CODEC},
            {"explode", ExplodeEffect.CODEC},
            {"ignite", Ignite.CODEC},
            {"apply_impulse", ApplyEntityImpulse.CODEC},
            {"apply_exhaustion", ApplyExhaustion.CODEC},
            {"play_sound", PlaySoundEffect.CODEC},
            {"replace_block", ReplaceBlock.CODEC},
            {"replace_disk", ReplaceDisk.CODEC},
            {"run_function", RunFunction.CODEC},
            {"set_block_properties", SetBlockProperties.CODEC},
            {"spawn_particles", SpawnParticlesEffect.CODEC},
        };
        for (Object[] entry : entries) {
            Registry.register((Registry) registry, (String) entry[0], entry[1]);
        }
        return (MapCodec) Registry.register((Registry) registry, "summon_entity", SummonEntityEffect.CODEC);
    }
    void onChangedBlock(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position, boolean becameActive);
    default void onDeactivated(EnchantedItemInUse item, Entity entity, Vec3 position, int level) {
    }
    MapCodec<? extends EnchantmentLocationBasedEffect> codec();
}
