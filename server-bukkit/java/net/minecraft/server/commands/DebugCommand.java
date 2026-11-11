// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collection;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import net.minecraft.util.Util;
import net.minecraft.commands.CommandResultCallback;
import net.minecraft.commands.FunctionInstantiationException;
import net.minecraft.commands.arguments.item.FunctionArgument;
import net.minecraft.commands.execution.ExecutionContext;
import net.minecraft.commands.execution.Frame;
import net.minecraft.commands.execution.tasks.CallFunction;
import net.minecraft.commands.functions.CommandFunction;
import net.minecraft.commands.functions.InstantiatedFunction;
import net.minecraft.server.permissions.LevelBasedPermissionSet;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.context.ContextChain;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.io.PrintWriter;
import net.minecraft.commands.CommandSource;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.execution.ChainModifiers;
import net.minecraft.commands.execution.CustomCommandExecutor;
import net.minecraft.commands.execution.ExecutionControl;
import net.minecraft.commands.execution.TraceCallbacks;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
public class DebugCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "debug");
    }
    private static final CommandResponseTracker.DispatchWithArg<Component, CommandFunction<CommandSourceStack>, String> kTraceResponse =
        new CommandResponseTracker.DispatchWithArg<>(
            (function, total, name) -> Component.translatable("commands.debug.function.success.single", total, Component.translationArg(function.id()), name),
            (count, total, name) -> Component.translatable("commands.debug.function.success.multiple", total, count, name));

    static class TraceCustomExecutor extends CustomCommandExecutor.WithErrorHandling<CommandSourceStack> implements CustomCommandExecutor.CommandAdapter<CommandSourceStack> {
        public void runGuarded(CommandSourceStack source, ContextChain<CommandSourceStack> currentStep, ChainModifiers modifiers, ExecutionControl<CommandSourceStack> context) throws CommandSyntaxException {
            if (modifiers.isReturn()) {
                throw new SimpleCommandExceptionType(Component.translatable("commands.debug.function.noReturnRun")).create();
            }
            if (context.tracer() != null) {
                throw new SimpleCommandExceptionType(Component.translatable("commands.debug.function.noRecursion")).create();
            }
            Collection<CommandFunction<CommandSourceStack>> functions = FunctionArgument.getFunctions(currentStep.getTopContext(), "name");
            String fileName = "debug-trace-" + Util.getFilenameFormattedDateTime() + ".txt";
            var dispatcher = source.getServer().getFunctions().getDispatcher();
            CommandResponseTracker<CommandFunction<CommandSourceStack>> tracker = CommandResponseTracker.create();
            try {
                Path directory = source.getServer().getFile("debug");
                Files.createDirectories(directory);
                PrintWriter file = new PrintWriter(Files.newBufferedWriter(directory.resolve(fileName), StandardCharsets.UTF_8));
                Tracer tracer = new Tracer(file);
                context.tracer(tracer);
                for (CommandFunction<CommandSourceStack> function : functions) {
                    try {
                        CommandSourceStack inner = source.withSource(tracer).withMaximumPermission(LevelBasedPermissionSet.GAMEMASTER);
                        InstantiatedFunction<CommandSourceStack> body = function.instantiate(null, dispatcher);
                        context.queueNext(new CallFunction<CommandSourceStack>(body, CommandResultCallback.EMPTY, false) {
                            public void execute(CommandSourceStack caller, ExecutionContext<CommandSourceStack> running, Frame frame) {
                                file.println(function.id());
                                super.execute(caller, running, frame);
                            }
                        }.bind(inner));
                        tracker.track(function, body.entries().size());
                    } catch (FunctionInstantiationException failure) {
                        source.sendFailure(failure.messageComponent());
                    }
                }
            } catch (IOException | UncheckedIOException failure) {
                source.sendFailure(Component.translatable("commands.debug.function.traceFailed"));
            }
            context.queueNext((done, frame) -> source.sendSuccess(() -> tracker.dispatch(CommandResponseTracker.ElementType.ANY, kTraceResponse, fileName), true));
        }
    }
    static class Tracer implements CommandSource, TraceCallbacks {
        public static final int INDENT_OFFSET = 1;
        private final PrintWriter output;
        private int lastIndent;
        private boolean waitingForResult;

        private Tracer(PrintWriter output) {
            this.output = output;
        }

        private void indent(int level) {
            output.write("    ".repeat(level + INDENT_OFFSET));
        }

        private void settle() {
            if (waitingForResult) {
                output.println();
                waitingForResult = false;
            }
        }

        private void mark(int depth, String tag) {
            settle();
            indent(depth);
            lastIndent = depth;
            output.print(tag);
        }

        public void onCommand(int depth, String command) {
            mark(depth, "[C] ");
            output.print(command);
            waitingForResult = true;
        }

        public void onReturn(int depth, String command, int result) {
            if (waitingForResult) {
                output.println(" -> " + result);
                waitingForResult = false;
                return;
            }
            mark(depth, "[R = " + result + "] ");
            output.println(command);
        }

        public void onCall(int depth, Identifier function, int size) {
            mark(depth, "[F] ");
            output.println(function + " size=" + size);
        }

        public void onError(String message) {
            mark(lastIndent + 1, "[E] ");
            output.print(message);
        }

        public void sendSystemMessage(Component message) {
            settle();
            indent(lastIndent + 1);
            output.println("[M] " + message.getString());
        }

        public boolean acceptsSuccess() {
            return true;
        }

        public boolean acceptsFailure() {
            return true;
        }

        public boolean shouldInformAdmins() {
            return false;
        }

        public boolean alwaysAccepts() {
            return true;
        }

        public void close() {
            output.close();
        }
    }
}
