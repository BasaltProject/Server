// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;

import com.mojang.serialization.MapCodec;
import java.util.function.ToIntFunction;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;

final class ComputedInt implements ContextIntProvider {
    private final ToIntFunction<LootContext> body;

    ComputedInt(ToIntFunction<LootContext> body) {
        this.body = body;
    }

    public int getIntUnsafe(LootContext context) {
        return body.applyAsInt(context);
    }

    public MapCodec<? extends ContextIntProvider> codec() {
        throw new UnsupportedOperationException("computed providers are not serialised");
    }

    public void validate(ValidationContext context) {
    }
}
