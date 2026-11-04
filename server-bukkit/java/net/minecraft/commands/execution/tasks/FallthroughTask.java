// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.execution.tasks;
import net.minecraft.commands.ExecutionCommandSource;
import net.minecraft.commands.execution.EntryAction;
import net.minecraft.commands.execution.ExecutionContext;
import net.minecraft.commands.execution.Frame;
public class FallthroughTask<T extends ExecutionCommandSource<T>> implements EntryAction<T> {
    private static final FallthroughTask<?> kInstance = new FallthroughTask<>();
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <T extends ExecutionCommandSource<T>> EntryAction<T> instance() {
        return (EntryAction) kInstance;
    }
    public void execute(ExecutionContext<T> context, Frame frame) {
        frame.returnFailure();
        frame.discard();
    }
}
