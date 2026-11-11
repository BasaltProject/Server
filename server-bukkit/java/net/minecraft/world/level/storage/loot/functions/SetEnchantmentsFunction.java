// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public class SetEnchantmentsFunction extends LootItemConditionalFunction {
    public static final MapCodec<SetEnchantmentsFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetEnchantmentsFunction.class, "MAP_CODEC");
    private final Map<Holder<Enchantment>, Holder<ContextIntProvider>> enchantments;
    private final boolean add;
    private SetEnchantmentsFunction(Optional<Holder<LootItemCondition>> condition, Map<Holder<Enchantment>, Holder<ContextIntProvider>> enchantments, boolean add) {
        super(condition);
        this.enchantments = enchantments;
        this.add = add;
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:set_enchantments\",\"add\":").append(add).append(",\"enchantments\":{");
        boolean first = true;
        for (Map.Entry<Holder<Enchantment>, Holder<ContextIntProvider>> entry : enchantments.entrySet()) {
            if (!first) {
                out.append(',');
            }
            first = false;
            org.basaltmc.bukkit.loot.LootJson.string(out, entry.getKey().getRegisteredName());
            out.append(':');
            org.basaltmc.bukkit.loot.LootJson.value(out, entry.getValue());
        }
        out.append('}');
    }
    public MapCodec<SetEnchantmentsFunction> codec() {
        return MAP_CODEC;
    }
    public void validate(ValidationContext context) {
        return;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        EnchantmentHelper.updateEnchantments(itemStack, mutable -> enchantments.forEach((enchantment, provider) -> {
            int level = provider.value().getInt(context);
            mutable.set(enchantment, add ? mutable.getLevel(enchantment) + level : level);
        }));
        return itemStack;
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private final Map<Holder<Enchantment>, Holder<ContextIntProvider>> enchantments = new LinkedHashMap<>();
        private final boolean add;
        public Builder() {
            this(false);
        }
        public Builder(boolean add) {
            this.add = add;
        }
        protected Builder getThis() {
            return this;
        }
        public Builder withEnchantment(Holder<Enchantment> enchantment, Holder<ContextIntProvider> levelProvider) {
            enchantments.put(enchantment, levelProvider);
            return this;
        }
        public LootItemFunction build() {
            return new SetEnchantmentsFunction(getCondition(), new LinkedHashMap<>(enchantments), add);
        }
    }
}
