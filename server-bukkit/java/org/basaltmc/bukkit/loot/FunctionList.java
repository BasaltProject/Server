// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package org.basaltmc.bukkit.loot;

import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;

/** Several functions applied in order, written as `sequence`. */
public final class FunctionList implements LootItemFunction, LootJson.Node {
    private final List<Holder<LootItemFunction>> steps;

    public FunctionList(List<Holder<LootItemFunction>> steps) {
        this.steps = List.copyOf(steps);
    }

    @Override
    public MapCodec<? extends LootItemFunction> codec() {
        return null;
    }

    @Override
    public ItemStack apply(ItemStack stack, LootContext context) {
        for (Holder<LootItemFunction> step : steps) {
            stack = step.value().apply(stack, context);
        }
        return stack;
    }

    @Override
    public void validate(ValidationContext context) {
    }

    @Override
    public void writeJson(StringBuilder out) {
        out.append("{\"type\":\"minecraft:sequence\",\"functions\":[");
        for (int i = 0; i < steps.size(); i++) {
            if (i > 0) {
                out.append(',');
            }
            LootJson.value(out, steps.get(i));
        }
        out.append("]}");
    }
}
