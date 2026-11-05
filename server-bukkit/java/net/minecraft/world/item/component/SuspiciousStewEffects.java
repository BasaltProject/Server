// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.TooltipFlag;
import net.minecraft.world.level.Level;
public record SuspiciousStewEffects(List<Entry> effects) implements ConsumableListener, TooltipProvider {
    public static final SuspiciousStewEffects EMPTY = new SuspiciousStewEffects(List.of());
    public static final int DEFAULT_DURATION = 160;
    public static final Codec<SuspiciousStewEffects> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.SuspiciousStewEffects.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, SuspiciousStewEffects> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.SuspiciousStewEffects.class, "STREAM_CODEC");
    public SuspiciousStewEffects withEffectAdded(Entry entry) {
        List<Entry> added = new java.util.ArrayList<>(effects);
        added.add(entry);
        return new SuspiciousStewEffects(List.copyOf(added));
    }
    public void onConsume(Level level, LivingEntity user, ItemStack stack, Consumable consumable) {
        for (Entry entry : effects) {
            user.addEffect(entry.createEffectInstance());
        }
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        if (flag.isCreative()) {
            java.util.List<MobEffectInstance> instances = new java.util.ArrayList<>();
            for (Entry entry : effects) {
                instances.add(entry.createEffectInstance());
            }
            net.minecraft.world.item.alchemy.PotionContents.addPotionTooltip(instances, consumer, 1.0F, context.tickRate());
        }
    }
    public static record Entry(Holder<MobEffect> effect, int duration) {
        public static final Codec<Entry> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.SuspiciousStewEffects.Entry.class, "CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, Entry> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.SuspiciousStewEffects.Entry.class, "STREAM_CODEC");
        public MobEffectInstance createEffectInstance() {
            return new MobEffectInstance(effect, duration);
        }
    }
}
