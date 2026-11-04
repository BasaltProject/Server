// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
public class RepairItemRecipe extends CustomRecipe {
    public static final RepairItemRecipe INSTANCE = new RepairItemRecipe();
    public static final MapCodec<RepairItemRecipe> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.RepairItemRecipe.class, "MAP_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, RepairItemRecipe> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.RepairItemRecipe.class, "STREAM_CODEC");
    public static final RecipeSerializer<RepairItemRecipe> SERIALIZER = null;
    public boolean matches(CraftingInput input, Level level) {
        return findPair(input) != null;
    }
    public ItemStack assemble(CraftingInput input) {
        ItemStack[] pair = findPair(input);
        if (pair == null) {
            return ItemStack.EMPTY;
        }
        ItemStack first = pair[0];
        ItemStack second = pair[1];
        int maxDamage = Math.max(first.getMaxDamage(), second.getMaxDamage());
        int remaining = first.getMaxDamage() - first.getDamageValue() + second.getMaxDamage() - second.getDamageValue() + maxDamage * 5 / 100;
        ItemStack out = new ItemStack(first.getItem());
        out.set(net.minecraft.core.component.DataComponents.MAX_DAMAGE, maxDamage);
        out.setDamageValue(Math.max(maxDamage - remaining, 0));
        net.minecraft.world.item.enchantment.ItemEnchantments a = net.minecraft.world.item.enchantment.EnchantmentHelper.getEnchantmentsForCrafting(first);
        net.minecraft.world.item.enchantment.ItemEnchantments b = net.minecraft.world.item.enchantment.EnchantmentHelper.getEnchantmentsForCrafting(second);
        net.minecraft.world.item.enchantment.EnchantmentHelper.updateEnchantments(out, enchantments -> {
            java.util.Set<net.minecraft.core.Holder<net.minecraft.world.item.enchantment.Enchantment>> all = new java.util.LinkedHashSet<>(a.keySet());
            all.addAll(b.keySet());
            for (net.minecraft.core.Holder<net.minecraft.world.item.enchantment.Enchantment> enchantment : all) {
                if (enchantment.is(net.minecraft.tags.EnchantmentTags.CURSE)) {
                    enchantments.set(enchantment, Math.max(a.getLevel(enchantment), b.getLevel(enchantment)));
                }
            }
        });
        return out;
    }
    public RecipeSerializer<RepairItemRecipe> getSerializer() {
        return SERIALIZER;
    }
    private static ItemStack[] findPair(CraftingInput input) {
        if (input.ingredientCount() != 2) {
            return null;
        }
        ItemStack first = null;
        for (ItemStack stack : input.items()) {
            if (stack.isEmpty()) {
                continue;
            }
            if (first == null) {
                first = stack;
            } else {
                boolean same = stack.is(first.getItem()) && first.getCount() == 1 && stack.getCount() == 1;
                boolean damageable = hasDurability(first) && hasDurability(stack);
                return same && damageable ? new ItemStack[] {first, stack} : null;
            }
        }
        return null;
    }
    private static boolean hasDurability(ItemStack stack) {
        return stack.has(net.minecraft.core.component.DataComponents.MAX_DAMAGE) && stack.has(net.minecraft.core.component.DataComponents.DAMAGE);
    }
}
