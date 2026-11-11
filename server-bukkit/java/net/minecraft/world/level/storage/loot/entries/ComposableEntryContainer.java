// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;
import java.util.function.Consumer;
import net.minecraft.world.level.storage.loot.LootContext;
public interface ComposableEntryContainer {
    ComposableEntryContainer ALWAYS_FALSE = (context, output) -> false;
    ComposableEntryContainer ALWAYS_TRUE = (context, output) -> true;
    boolean expand(LootContext context, Consumer<LootPoolEntry> output);
    default ComposableEntryContainer and(ComposableEntryContainer other) {
        return (context, output) -> expand(context, output) && other.expand(context, output);
    }
    default ComposableEntryContainer or(ComposableEntryContainer other) {
        return (context, output) -> expand(context, output) || other.expand(context, output);
    }
}
