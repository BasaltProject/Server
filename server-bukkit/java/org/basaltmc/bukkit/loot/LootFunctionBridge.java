// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package org.basaltmc.bukkit.loot;

import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.nbt.TagParser;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import org.basaltmc.bukkit.LeftoverNatives;

/** Applies a facade loot function to a stack in the native loot code. */
public final class LootFunctionBridge {
    private LootFunctionBridge() {
    }

    public static ItemStack apply(net.minecraft.world.level.storage.loot.functions.LootItemConditionalFunction function, ItemStack stack, LootContext context) {
        if (stack.isEmpty()) {
            return stack;
        }
        CompoundTag encoded = (CompoundTag) ItemStack.CODEC.encodeStart(NbtOps.INSTANCE, stack)
            .getOrThrow(message -> new IllegalStateException("Cannot read the stack: " + message));
        Tag components = encoded.get("components");
        net.minecraft.world.phys.Vec3 origin = context.getOptional(LootContextParams.ORIGIN);
        String[] made = LeftoverNatives.applyLoot(
            new String[] {encoded.getStringOr("id", ""), String.valueOf(encoded.getIntOr("count", 1)),
                components == null ? "" : components.toString()},
            context.getLevel().getWorld().getName(), function.bareJson(), origin == null ? 0.0 : origin.x(),
            origin == null ? 0.0 : origin.y(), origin == null ? 0.0 : origin.z(), context.getRandom().nextLong());
        if (made == null) {
            throw new IllegalStateException("The server could not apply " + function.bareJson());
        }
        if (made[0].isEmpty() || made[1].equals("0")) {
            return ItemStack.EMPTY;
        }
        try {
            CompoundTag result = new CompoundTag();
            result.putString("id", made[0]);
            result.putInt("count", Integer.parseInt(made[1]));
            if (!made[2].isEmpty()) {
                result.put("components", TagParser.parseCompoundFully(made[2]));
            }
            return ItemStack.CODEC.parse(NbtOps.INSTANCE, result)
                .getOrThrow(message -> new IllegalStateException("Cannot read the result: " + message));
        } catch (com.mojang.brigadier.exceptions.CommandSyntaxException failure) {
            throw new IllegalStateException("The server returned bad components: " + made[2], failure);
        }
    }
}
