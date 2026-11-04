// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands;
import net.minecraft.server.MinecraftServer;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.ParseResults;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.builder.LiteralArgumentBuilder;
import com.mojang.brigadier.builder.RequiredArgumentBuilder;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.function.UnaryOperator;
import net.minecraft.commands.execution.ExecutionContext;
import net.minecraft.core.HolderLookup;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.permissions.PermissionCheck;
import net.minecraft.server.permissions.PermissionProviderCheck;
import net.minecraft.server.permissions.PermissionSet;
import net.minecraft.server.permissions.PermissionSetSupplier;
public class Commands {
    private final CommandDispatcher<CommandSourceStack> dispatcher = new CommandDispatcher<>() {
        @Override
        public com.mojang.brigadier.tree.LiteralCommandNode<CommandSourceStack> register(LiteralArgumentBuilder<CommandSourceStack> command) {
            com.mojang.brigadier.tree.LiteralCommandNode<CommandSourceStack> node = super.register(command);
            org.basaltmc.bukkit.facade.BukkitCommandBridge.expose(this, node, MinecraftServer.getServer());
            return node;
        }
    };
    public static final String COMMAND_PREFIX = "/";
    public static final PermissionCheck LEVEL_ALL = PermissionCheck.AlwaysPass.INSTANCE;
    public static final PermissionCheck LEVEL_MODERATORS = new PermissionCheck.Require(net.minecraft.server.permissions.Permissions.COMMANDS_MODERATOR);
    public static final PermissionCheck LEVEL_GAMEMASTERS = new PermissionCheck.Require(net.minecraft.server.permissions.Permissions.COMMANDS_GAMEMASTER);
    public static final PermissionCheck LEVEL_ADMINS = new PermissionCheck.Require(net.minecraft.server.permissions.Permissions.COMMANDS_ADMIN);
    public static final PermissionCheck LEVEL_OWNERS = new PermissionCheck.Require(net.minecraft.server.permissions.Permissions.COMMANDS_OWNER);
    public Commands(CommandSelection commandSelection, CommandBuildContext context) {
    }
    public static <S> ParseResults<S> mapSource(ParseResults<S> parse, UnaryOperator<S> sourceOperator) {
        com.mojang.brigadier.context.CommandContextBuilder<S> context = parse.getContext();
        return new ParseResults<>(context.withSource(sourceOperator.apply(context.getSource())), parse.getReader(), parse.getExceptions());
    }
    public void performPrefixedCommand(CommandSourceStack sender, String command) {
        performCommand(getDispatcher().parse(trimOptionalPrefix(command), sender), command);
    }
    public static String trimOptionalPrefix(String command) {
        return command.startsWith(COMMAND_PREFIX) ? command.substring(COMMAND_PREFIX.length()) : command;
    }
    private static CommandSyntaxException parseException(ParseResults<CommandSourceStack> parse) {
        var reader = parse.getReader();
        if (parse.getExceptions().size() == 1) {
            return parse.getExceptions().values().iterator().next();
        }
        return parse.getContext().getRange().isEmpty() ? CommandSyntaxException.BUILT_IN_EXCEPTIONS.dispatcherUnknownCommand().createWithContext(reader)
            : CommandSyntaxException.BUILT_IN_EXCEPTIONS.dispatcherUnknownArgument().createWithContext(reader);
    }
    public void performCommand(ParseResults<CommandSourceStack> command, String commandString) {
        CommandSourceStack source = command.getContext().getSource();
        try {
            var chain = com.mojang.brigadier.context.ContextChain.tryFlatten(command.getContext().build(commandString));
            if (chain.isEmpty()) {
                throw parseException(command);
            }
            executeCommandInContext(source, context -> ExecutionContext.queueInitialCommandExecution(context, commandString, chain.get(), source, CommandResultCallback.EMPTY));
        } catch (CommandSyntaxException e) {
            source.sendFailure(net.minecraft.network.chat.Component.literal(e.getMessage()));
        } catch (RuntimeException failure) {
            source.sendFailure(net.minecraft.network.chat.Component.translatable("command.failed"));
        }
    }
    private static final ThreadLocal<ExecutionContext<CommandSourceStack>> kCurrentExecutionContext = new ThreadLocal<>();
    public static void executeCommandInContext(CommandSourceStack context, Consumer<ExecutionContext<CommandSourceStack>> config) {
        ExecutionContext<CommandSourceStack> current = kCurrentExecutionContext.get();
        if (current != null) {
            config.accept(current);
            return;
        }
        var rules = context.getLevel().getGameRules();
        int chainLimit = Math.max(1, rules.get(net.minecraft.world.level.gamerules.GameRules.MAX_COMMAND_SEQUENCE_LENGTH));
        int forkLimit = rules.get(net.minecraft.world.level.gamerules.GameRules.MAX_COMMAND_FORKS);
        try (ExecutionContext<CommandSourceStack> executionContext = new ExecutionContext<>(chainLimit, forkLimit, net.minecraft.util.profiling.Profiler.get())) {
            kCurrentExecutionContext.set(executionContext);
            config.accept(executionContext);
            executionContext.runCommandQueue();
        } finally {
            kCurrentExecutionContext.set(null);
        }
    }
    public void sendCommands(ServerPlayer player) {
        if (player.getBukkitEntity() instanceof org.bukkit.entity.Player bukkit) {
            bukkit.updateCommands();
        }
    }
    public static LiteralArgumentBuilder<CommandSourceStack> literal(String literal) {
        return LiteralArgumentBuilder.literal(literal);
    }
    public static <T> RequiredArgumentBuilder<CommandSourceStack, T> argument(String name, ArgumentType<T> type) {
        return RequiredArgumentBuilder.argument(name, type);
    }
    public static Predicate<String> createValidator(ParseFunction parser) {
        return value -> {
            try {
        parser.parse(new StringReader(value));
        return true;
            } catch (CommandSyntaxException e) {
        return false;
            }
        };
    }
    public CommandDispatcher<CommandSourceStack> getDispatcher() {
        return dispatcher;
    }
    public static <S> void validateParseResults(ParseResults<S> command) throws CommandSyntaxException {
        CommandSyntaxException exception = getParseException(command);
        if (exception != null) {
            throw exception;
        }
    }
    public static <S> CommandSyntaxException getParseException(ParseResults<S> parse) {
        if (!parse.getReader().canRead()) {
            return null;
        }
        if (parse.getExceptions().size() == 1) {
            return parse.getExceptions().values().iterator().next();
        }
        return parse.getContext().getRange().isEmpty() ? CommandSyntaxException.BUILT_IN_EXCEPTIONS.dispatcherUnknownCommand().createWithContext(parse.getReader())
            : CommandSyntaxException.BUILT_IN_EXCEPTIONS.dispatcherUnknownArgument().createWithContext(parse.getReader());
    }
    public static CommandBuildContext createValidationContext(HolderLookup.Provider registries) {
        return CommandBuildContext.simple(registries, net.minecraft.world.flag.FeatureFlags.REGISTRY.allFlags());
    }
    public static void validate() {
    }
    public static <T extends PermissionSetSupplier> PermissionProviderCheck<T> hasPermission(PermissionCheck permission) {
        return new PermissionProviderCheck<>(permission);
    }
    public static CommandSourceStack createCompilationContext(PermissionSet compilationPermissions) {
        return new CommandSourceStack(net.minecraft.commands.CommandSource.NULL, net.minecraft.world.phys.Vec3.ZERO,
            net.minecraft.world.phys.Vec2.ZERO, null, compilationPermissions, net.minecraft.network.chat.Component.literal("Compilation"), null);
    }
    public static enum CommandSelection {
        ALL, DEDICATED, INTEGRATED;
    }
    public interface ParseFunction {
        void parse(StringReader value) throws CommandSyntaxException;
    }
}
