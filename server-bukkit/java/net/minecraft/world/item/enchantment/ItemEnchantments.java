// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment;
import com.mojang.serialization.Codec;
import it.unimi.dsi.fastutil.objects.Object2IntMap;
import it.unimi.dsi.fastutil.objects.Object2IntOpenHashMap;
import java.util.Set;
import java.util.function.Consumer;
import java.util.function.Predicate;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.TooltipFlag;
import net.minecraft.world.item.component.TooltipProvider;
public class ItemEnchantments implements TooltipProvider {
    private final Object2IntOpenHashMap<Holder<Enchantment>> enchantments;
    public static final ItemEnchantments EMPTY = new ItemEnchantments(new Object2IntOpenHashMap<>());
    public static final Codec<ItemEnchantments> CODEC = Codec.unboundedMap(
        net.minecraft.resources.Identifier.CODEC.<Holder<Enchantment>>xmap(
            id -> org.basaltmc.bukkit.facade.HolderRefs.of(Enchantment.class, id), holder -> holder.unwrapKey().get().identifier()),
        Codec.intRange(1, 255)).xmap(map -> {
            Object2IntOpenHashMap<Holder<Enchantment>> out = new Object2IntOpenHashMap<>();
            map.forEach(out::put);
            return new ItemEnchantments(out);
        }, enchantments -> {
            java.util.Map<Holder<Enchantment>, Integer> out = new java.util.LinkedHashMap<>();
            enchantments.enchantments.forEach(out::put);
            return out;
        });
    public static final StreamCodec<RegistryFriendlyByteBuf, ItemEnchantments> STREAM_CODEC = net.minecraft.network.codec.StreamCodec.composite(net.minecraft.network.codec.ByteBufCodecs.map(Object2IntOpenHashMap::new, Enchantment.STREAM_CODEC, net.minecraft.network.codec.ByteBufCodecs.VAR_INT), each -> each.enchantments, ItemEnchantments::new);
    private ItemEnchantments(Object2IntOpenHashMap<Holder<Enchantment>> enchantments) {
        this.enchantments = enchantments;
    }
    public int getLevel(Holder<Enchantment> enchantment) {
        return enchantments.getInt(enchantment);
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {

    }
    public Set<Holder<Enchantment>> keySet() {
        return java.util.Collections.unmodifiableSet(enchantments.keySet());
    }
    public Set<Object2IntMap.Entry<Holder<Enchantment>>> entrySet() {
        return java.util.Collections.unmodifiableSet(enchantments.object2IntEntrySet());
    }
    public int size() {
        return enchantments.size();
    }
    public boolean isEmpty() {
        return enchantments.isEmpty();
    }
    public boolean equals(Object obj) {
        return obj instanceof ItemEnchantments other && enchantments.equals(other.enchantments);
    }
    public int hashCode() {
        return enchantments.hashCode();
    }
    public String toString() {
        return "ItemEnchantments{enchantments=" + enchantments + "}";
    }
    public static class Mutable {
        private final Object2IntOpenHashMap<Holder<Enchantment>> map;
        public Mutable(ItemEnchantments enchantments) {
            this.map = new Object2IntOpenHashMap<>(enchantments.enchantments);
        }
        public void set(Holder<Enchantment> enchantment, int level) {
            if (level <= 0) {
                map.removeInt(enchantment);
            } else {
                map.put(enchantment, Math.min(255, level));
            }
        }
        public void upgrade(Holder<Enchantment> enchantment, int level) {
            if (level > 0) {
                map.merge(enchantment, Math.min(255, level), Math::max);
            }
        }
        public void removeIf(Predicate<Holder<Enchantment>> predicate) {
            map.keySet().removeIf(predicate);
        }
        public int getLevel(Holder<Enchantment> enchantment) {
            return map.getInt(enchantment);
        }
        public Set<Holder<Enchantment>> keySet() {
            return map.keySet();
        }
        public ItemEnchantments toImmutable() {
            return new ItemEnchantments(new Object2IntOpenHashMap<>(map));
        }
    }
}
