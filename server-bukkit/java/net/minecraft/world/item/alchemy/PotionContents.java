// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.alchemy;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.tags.TagKey;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.TooltipFlag;
import net.minecraft.world.item.component.Consumable;
import net.minecraft.world.item.component.ConsumableListener;
import net.minecraft.world.item.component.TooltipProvider;
import net.minecraft.world.level.Level;
public record PotionContents(Optional<Holder<Potion>> potion, Optional<Integer> customColor, List<MobEffectInstance> customEffects, Optional<String> customName) implements ConsumableListener, TooltipProvider {
    public static final PotionContents EMPTY = new PotionContents(Optional.empty(), Optional.empty(), List.of(), Optional.empty());
    public static final int BASE_POTION_COLOR = -13083194;
    public static final Codec<PotionContents> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.alchemy.PotionContents.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, PotionContents> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.alchemy.PotionContents.class, "STREAM_CODEC");
    public PotionContents(Holder<Potion> potion) {
        this(Optional.of(potion), Optional.empty(), List.of(), Optional.empty());
    }
    public static ItemStack createItemStack(Item item, Holder<Potion> potion) {
        ItemStack stack = new ItemStack(item);
        stack.set(net.minecraft.core.component.DataComponents.POTION_CONTENTS, new PotionContents(potion));
        return stack;
    }
    public boolean is(Holder<Potion> potion) {
        return isPotionWithoutCustomEffects() && this.potion.get().is(potion);
    }
    public boolean is(TagKey<Potion> potion) {
        return isPotionWithoutCustomEffects() && this.potion.get().is(potion);
    }
    public boolean isPotionWithoutCustomEffects() {
        return potion.isPresent() && customEffects.isEmpty();
    }
    public Iterable<MobEffectInstance> getAllEffects() {
        if (potion.isEmpty()) {
            return customEffects;
        }
        List<MobEffectInstance> all = new java.util.ArrayList<>(potion.get().value().getEffects());
        all.addAll(customEffects);
        return all;
    }
    public void forEachEffect(Consumer<MobEffectInstance> consumer, float durationScale) {
        if (potion.isPresent()) {
            for (MobEffectInstance effect : potion.get().value().getEffects()) {
                consumer.accept(effect.withScaledDuration(durationScale));
            }
        }
        for (MobEffectInstance effect : customEffects) {
            consumer.accept(effect.withScaledDuration(durationScale));
        }
    }
    public PotionContents withPotion(Holder<Potion> potion) {
        return new PotionContents(Optional.of(potion), customColor, customEffects, customName);
    }
    public PotionContents withEffectAdded(MobEffectInstance effect) {
        List<MobEffectInstance> added = new java.util.ArrayList<>(customEffects);
        added.add(effect);
        return new PotionContents(potion, customColor, List.copyOf(added), customName);
    }
    public int getColor() {
        return getColorOr(BASE_POTION_COLOR);
    }
    public int getColorOr(int defaultColor) {
        return customColor.isPresent() ? customColor.get() : getColorOptional(getAllEffects()).orElse(defaultColor);
    }
    public Component getName(String prefix) {
        String suffix = customName.or(() -> potion.map(held -> held.value().name())).orElse("empty");
        return Component.translatable(prefix + suffix);
    }
    public static OptionalInt getColorOptional(Iterable<MobEffectInstance> effects) {
        int red = 0;
        int green = 0;
        int blue = 0;
        int weight = 0;
        for (MobEffectInstance effect : effects) {
            if (effect.isVisible()) {
                int color = effect.getEffect().value().getColor();
                int scale = effect.getAmplifier() + 1;
                red += scale * net.minecraft.util.ARGB.red(color);
                green += scale * net.minecraft.util.ARGB.green(color);
                blue += scale * net.minecraft.util.ARGB.blue(color);
                weight += scale;
            }
        }
        return weight == 0 ? OptionalInt.empty() : OptionalInt.of(net.minecraft.util.ARGB.color(red / weight, green / weight, blue / weight));
    }
    public boolean hasEffects() {
        return !customEffects.isEmpty() || potion.isPresent() && !potion.get().value().getEffects().isEmpty();
    }
    public List<MobEffectInstance> customEffects() {
        return customEffects;
    }
    public void applyToLivingEntity(LivingEntity entity, float durationScale) {
        if (!(entity.level() instanceof net.minecraft.server.level.ServerLevel serverLevel)) {
            return;
        }
        net.minecraft.world.entity.player.Player player = entity instanceof net.minecraft.world.entity.player.Player found ? found : null;
        forEachEffect(effect -> {
            if (effect.getEffect().value().isInstantaneous()) {
                effect.getEffect().value().applyInstantaneousEffect(serverLevel, player, player, entity, effect.getAmplifier(), 1.0);
            } else {
                entity.addEffect(effect);
            }
        }, durationScale);
    }
    public static void addPotionTooltip(Iterable<MobEffectInstance> effects, Consumer<Component> lines, float durationScale, float tickrate) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.alchemy.PotionContents#addPotionTooltip");
    }
    public static MutableComponent getPotionDescription(Holder<MobEffect> mobEffect, int amplifier) {
        MutableComponent line = Component.translatable(mobEffect.value().getDescriptionId());
        return amplifier > 0 ? Component.translatable("potion.withAmplifier", line, Component.translatable("potion.potency." + amplifier)) : line;
    }
    public void onConsume(Level level, LivingEntity user, ItemStack stack, Consumable consumable) {
        applyToLivingEntity(user, stack.getOrDefault(net.minecraft.core.component.DataComponents.POTION_DURATION_SCALE, 1.0F));
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.alchemy.PotionContents#addToTooltip");
    }
}
