// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import it.unimi.dsi.fastutil.objects.Object2IntMap;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.item.enchantment.ItemEnchantments;
import org.basaltmc.bukkit.facade.ReflectiveCodecs;
public record EnchantmentPredicate(Optional<HolderSet<Enchantment>> enchantments, MinMaxBounds.Ints level) {
    public static final Codec<EnchantmentPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        ReflectiveCodecs.holderSet(Enchantment.class).optionalFieldOf("enchantments").forGetter(EnchantmentPredicate::enchantments),
        MinMaxBounds.Ints.CODEC.optionalFieldOf("levels", MinMaxBounds.Ints.ANY).forGetter(EnchantmentPredicate::level)).apply(i, EnchantmentPredicate::new));
    public EnchantmentPredicate(Holder<Enchantment> enchantment, MinMaxBounds.Ints level) {
        this(Optional.of(HolderSet.direct(enchantment)), level);
    }
    public EnchantmentPredicate(HolderSet<Enchantment> enchantments, MinMaxBounds.Ints level) {
        this(Optional.of(enchantments), level);
    }
    public boolean containedIn(ItemEnchantments itemEnchantments) {
        if (enchantments.isPresent()) {
            for (Holder<Enchantment> enchantment : enchantments.get()) {
                if (matchesEnchantment(itemEnchantments, enchantment)) {
                    return true;
                }
            }
            return false;
        }
        if (level != MinMaxBounds.Ints.ANY) {
            for (Object2IntMap.Entry<Holder<Enchantment>> entry : itemEnchantments.entrySet()) {
                if (level.matches(entry.getIntValue())) {
                    return true;
                }
            }
            return false;
        }
        return !itemEnchantments.isEmpty();
    }
    private boolean matchesEnchantment(ItemEnchantments itemEnchantments, Holder<Enchantment> enchantment) {
        int applied = itemEnchantments.getLevel(enchantment);
        return applied != 0 && (level == MinMaxBounds.Ints.ANY || level.matches(applied));
    }
}
