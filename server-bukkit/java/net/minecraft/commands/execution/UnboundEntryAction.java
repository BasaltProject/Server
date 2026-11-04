// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.execution;
@FunctionalInterface
public interface UnboundEntryAction<T> {
    void execute(T sender, ExecutionContext<T> context, Frame frame);
    default EntryAction<T> bind(T sender) {
        return (context, frame) -> execute(sender, context, frame);
    }
}
