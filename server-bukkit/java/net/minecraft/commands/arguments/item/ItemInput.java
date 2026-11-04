// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.item;

import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.network.chat.Component;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;

public record ItemInput(Holder<Item> item, DataComponentPatch components) {
    private static final Dynamic2CommandExceptionType ERROR_STACK_TOO_BIG = new Dynamic2CommandExceptionType(
        (item, max) -> new com.mojang.brigadier.LiteralMessage("Can't give more than " + max + " of " + item));

    public ItemStack createItemStack(int count) throws CommandSyntaxException {
        ItemStack stack = new ItemStack(item, count);
        stack.applyComponents(components);
        int max = stack.getMaxStackSize();
        if (count > max) {
            throw ERROR_STACK_TOO_BIG.create(item.value(), max);
        }
        return stack;
    }
}
