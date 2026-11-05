// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.item.enchantment.LevelBasedValue;
public class ContextFloatProviders {
    public static final Codec<ContextFloatProvider> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders.class, "DIRECT_CODEC");
    public static final Codec<Holder<ContextFloatProvider>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders.class, "CODEC");
    public static final Codec<HolderSet<ContextFloatProvider>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders.class, "LIST_CODEC");
    public static final ResourceKey<ContextFloatProvider> COOKING_DEFAULT_SPEED_MULTIPLIER = null;
    public static final ResourceKey<ContextFloatProvider> COOKING_NORMAL_SPEED_MULTIPLIER = null;
    public static final ResourceKey<ContextFloatProvider> COOKING_FAST_SPEED_MULTIPLIER = null;
    public static final ResourceKey<ContextFloatProvider> BREWING_DEFAULT_SPEED_MULTIPLIER = null;
    public static void bootstrap(BootstrapContext<ContextFloatProvider> context) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#bootstrap");
    }
    public static Holder<ContextFloatProvider> abs(Holder<ContextFloatProvider> input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#abs");
    }
    public static Holder<ContextFloatProvider> avg(Holder<ContextFloatProvider>... inputs) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#avg");
    }
    public static Holder<ContextFloatProvider> ceiling(Holder<ContextFloatProvider> input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#ceiling");
    }
    public static Holder<ContextFloatProvider> exactly(float value) {
        return Holder.direct(new ConstantValue(value));
    }
    public static Holder<ContextFloatProvider> cos(Holder<ContextFloatProvider> input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#cos");
    }
    public static Holder<ContextFloatProvider> sub(Holder<ContextFloatProvider> left, Holder<ContextFloatProvider> right) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#sub");
    }
    public static Holder<ContextFloatProvider> forEnchantmentLevel(LevelBasedValue amount) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#forEnchantmentLevel");
    }
    public static Holder<ContextFloatProvider> forEnvironmentAttribute(EnvironmentAttribute<?> attribute) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#forEnvironmentAttribute");
    }
    public static Holder<ContextFloatProvider> length(Holder<ContextFloatProvider>... inputs) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#length");
    }
    public static Holder<ContextFloatProvider> max(Holder<ContextFloatProvider>... inputs) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#max");
    }
    public static Holder<ContextFloatProvider> min(Holder<ContextFloatProvider>... inputs) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#min");
    }
    public static Holder<ContextFloatProvider> mod(Holder<ContextFloatProvider> left, Holder<ContextFloatProvider> right) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#mod");
    }
    public static Holder<ContextFloatProvider> negate(Holder<ContextFloatProvider> input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#negate");
    }
    public static Holder<ContextFloatProvider> pow(Holder<ContextFloatProvider> base, Holder<ContextFloatProvider> exponent) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#pow");
    }
    public static Holder<ContextFloatProvider> mul(Holder<ContextFloatProvider>... inputs) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#mul");
    }
    public static Holder<ContextFloatProvider> div(Holder<ContextFloatProvider> left, Holder<ContextFloatProvider> right) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#div");
    }
    public static Holder<ContextFloatProvider> round(Holder<ContextFloatProvider> input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#round");
    }
    public static Holder<ContextFloatProvider> sin(Holder<ContextFloatProvider> input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#sin");
    }
    public static Holder<ContextFloatProvider> sqrt(Holder<ContextFloatProvider> input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#sqrt");
    }
    public static Holder<ContextFloatProvider> add(Holder<ContextFloatProvider>... inputs) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#add");
    }
    public static Holder<ContextFloatProvider> trunc(Holder<ContextFloatProvider> input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders#trunc");
    }
    public static Holder<ContextFloatProvider> between(float min, float max) {
        return Holder.direct(new UniformGenerator(exactly(min), exactly(max)));
    }
}
