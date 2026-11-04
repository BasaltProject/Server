// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands;

import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.LiteralMessage;
import com.mojang.brigadier.Message;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandExceptionType;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.function.BinaryOperator;
import java.util.function.Predicate;
import java.util.function.Supplier;
import java.util.stream.Stream;
import net.minecraft.commands.arguments.EntityAnchorArgument;
import net.minecraft.commands.execution.TraceCallbacks;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.network.chat.ChatType;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.OutgoingChatMessage;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.permissions.PermissionSet;
import net.minecraft.util.TaskChainer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;

public class CommandSourceStack implements SharedSuggestionProvider, ExecutionCommandSource<CommandSourceStack> {
    public static final SimpleCommandExceptionType ERROR_NOT_PLAYER =
        new SimpleCommandExceptionType(new LiteralMessage("A player is required to run this command here"));
    public static final SimpleCommandExceptionType ERROR_NOT_ENTITY =
        new SimpleCommandExceptionType(new LiteralMessage("An entity is required to run this command here"));
    private final CommandSource source;
    private final Vec3 position;
    private final ServerLevel level;
    private final PermissionSet permissions;
    private final NamesProvider namesProvider;
    private final MinecraftServer server;
    private final Entity entity;
    private final boolean silent;
    private final CommandResultCallback callback;
    private final EntityAnchorArgument.Anchor anchor;
    private final Vec2 rotation;
    private final CommandSigningContext signingContext;
    private final TaskChainer chatMessageChainer;
    private CommandDispatcher<CommandSourceStack> dispatcher;

    public CommandSourceStack(CommandSource source, Vec3 position, Vec2 rotation, ServerLevel level,
        PermissionSet permissions, MinecraftServer server, Entity entity) {
        this(source, position, rotation, level, permissions, entity == null ? NamesProvider.constant(Component.literal("Server"))
            : NamesProvider.FOR_ENTITY, server, entity);
    }

    public CommandSourceStack(CommandSource source, Vec3 position, Vec2 rotation, ServerLevel level,
        PermissionSet permissions, Component name, MinecraftServer server) {
        this(source, position, rotation, level, permissions, NamesProvider.constant(name), server, null);
    }

    private CommandSourceStack(CommandSource source, Vec3 position, Vec2 rotation, ServerLevel level,
        PermissionSet permissions, NamesProvider namesProvider, MinecraftServer server, Entity entity) {
        this(source, position, rotation, level, permissions, namesProvider, server, entity, false,
            CommandResultCallback.EMPTY, EntityAnchorArgument.Anchor.FEET, CommandSigningContext.ANONYMOUS, null);
    }

    private CommandSourceStack(CommandSource source, Vec3 position, Vec2 rotation, ServerLevel level,
        PermissionSet permissions, NamesProvider namesProvider, MinecraftServer server, Entity entity, boolean silent,
        CommandResultCallback callback, EntityAnchorArgument.Anchor anchor, CommandSigningContext signingContext,
        TaskChainer chatMessageChainer) {
        this.source = source;
        this.position = position;
        this.rotation = rotation;
        this.level = level;
        this.permissions = permissions;
        this.namesProvider = namesProvider;
        this.server = server;
        this.entity = entity;
        this.silent = silent;
        this.callback = callback;
        this.anchor = anchor;
        this.signingContext = signingContext;
        this.chatMessageChainer = chatMessageChainer;
    }

    private CommandSourceStack copy(CommandSource source, Vec3 position, Vec2 rotation, ServerLevel level,
        PermissionSet permissions, Entity entity, boolean silent, CommandResultCallback callback,
        EntityAnchorArgument.Anchor anchor, CommandSigningContext signingContext, TaskChainer chatMessageChainer) {
        CommandSourceStack stack = new CommandSourceStack(source, position, rotation, level, permissions, namesProvider,
            server, entity, silent, callback, anchor, signingContext, chatMessageChainer);
        stack.dispatcher = dispatcher;
        return stack;
    }

    public CommandSource getCommandSource() {
        return source;
    }

    public CommandSourceStack withSource(CommandSource source) {
        return this.source == source ? this : copy(source, position, rotation, level, permissions, entity, silent, callback,
            anchor, signingContext, chatMessageChainer);
    }

    public CommandSourceStack withEntity(Entity entity) {
        return this.entity == entity ? this : copy(source, position, rotation, level, permissions, entity, silent, callback,
            anchor, signingContext, chatMessageChainer);
    }

    public CommandSourceStack withPosition(Vec3 pos) {
        return position.equals(pos) ? this : copy(source, pos, rotation, level, permissions, entity, silent, callback, anchor,
            signingContext, chatMessageChainer);
    }

    public CommandSourceStack withRotation(Vec2 rotation) {
        return this.rotation.equals(rotation) ? this : copy(source, position, rotation, level, permissions, entity, silent,
            callback, anchor, signingContext, chatMessageChainer);
    }

    public CommandSourceStack withCallback(CommandResultCallback resultCallback) {
        return callback == resultCallback ? this : copy(source, position, rotation, level, permissions, entity, silent,
            resultCallback, anchor, signingContext, chatMessageChainer);
    }

    public CommandSourceStack withCallback(CommandResultCallback newCallback, BinaryOperator<CommandResultCallback> combiner) {
        return withCallback(combiner.apply(callback, newCallback));
    }

    public CommandSourceStack withSuppressedOutput() {
        return silent ? this : copy(source, position, rotation, level, permissions, entity, true, callback, anchor,
            signingContext, chatMessageChainer);
    }

    public CommandSourceStack withPermission(PermissionSet permissions) {
        return this.permissions == permissions ? this : copy(source, position, rotation, level, permissions, entity, silent,
            callback, anchor, signingContext, chatMessageChainer);
    }

    public CommandSourceStack withMaximumPermission(PermissionSet newPermissions) {
        return withPermission(permissions.union(newPermissions));
    }

    public CommandSourceStack withAnchor(EntityAnchorArgument.Anchor anchor) {
        return this.anchor == anchor ? this : copy(source, position, rotation, level, permissions, entity, silent, callback,
            anchor, signingContext, chatMessageChainer);
    }

    public CommandSourceStack withLevel(ServerLevel level) {
        return this.level == level ? this : copy(source, position, rotation, level, permissions, entity, silent, callback,
            anchor, signingContext, chatMessageChainer);
    }

    public CommandSourceStack facing(Entity entity, EntityAnchorArgument.Anchor anchor) {
        return facing(anchor.apply(entity));
    }

    public CommandSourceStack facing(Vec3 pos) {
        double dx = pos.x - position.x;
        double dy = pos.y - position.y;
        double dz = pos.z - position.z;
        double horizontal = Math.sqrt(dx * dx + dz * dz);
        float xRot = net.minecraft.util.Mth.wrapDegrees((float) -(Math.toDegrees(Math.atan2(dy, horizontal))));
        float yRot = net.minecraft.util.Mth.wrapDegrees((float) (Math.toDegrees(Math.atan2(dz, dx)) - 90.0));
        return withRotation(new Vec2(xRot, yRot));
    }

    public CommandSourceStack withSigningContext(CommandSigningContext signingContext, TaskChainer chatMessageChainer) {
        return copy(source, position, rotation, level, permissions, entity, silent, callback, anchor, signingContext,
            chatMessageChainer);
    }

    public Component getDisplayName() {
        return namesProvider.displayName(entity);
    }

    public String getTextName() {
        return namesProvider.textName(entity);
    }

    @Override
    public PermissionSet permissions() {
        return permissions;
    }

    public Vec3 getPosition() {
        return position;
    }

    public ServerLevel getLevel() {
        return level;
    }

    public Entity getEntity() {
        return entity;
    }

    public Entity getEntityOrException() throws CommandSyntaxException {
        if (entity == null) {
            throw ERROR_NOT_ENTITY.create();
        }
        return entity;
    }

    public ServerPlayer getPlayerOrException() throws CommandSyntaxException {
        if (entity instanceof ServerPlayer player) {
            return player;
        }
        throw ERROR_NOT_PLAYER.create();
    }

    public ServerPlayer getPlayer() {
        return entity instanceof ServerPlayer player ? player : null;
    }

    public boolean isPlayer() {
        return entity instanceof ServerPlayer;
    }

    public Vec2 getRotation() {
        return rotation;
    }

    public MinecraftServer getServer() {
        return server;
    }

    public EntityAnchorArgument.Anchor getAnchor() {
        return anchor;
    }

    public CommandSigningContext getSigningContext() {
        return signingContext;
    }

    public TaskChainer getChatMessageChainer() {
        return chatMessageChainer;
    }

    public boolean shouldFilterMessageTo(ServerPlayer receiver) {
        return false;
    }

    public void sendChatMessage(OutgoingChatMessage message, boolean filtered, ChatType.Bound chatType) {
        if (!silent) {
            source.sendSystemMessage(message.content());
        }
    }

    public void sendSystemMessage(Component message) {
        if (!silent) {
            source.sendSystemMessage(message);
        }
    }

    public void sendSuccess(Supplier<Component> messageSupplier, boolean broadcast) {
        boolean show = source.acceptsSuccess() && !silent;
        if (show) {
            source.sendSystemMessage(messageSupplier.get());
        }
    }

    public void sendFailure(Component message) {
        if (source.acceptsFailure() && !silent) {
            source.sendSystemMessage(Component.empty().append(message).withStyle(net.minecraft.ChatFormatting.RED));
        }
    }

    @Override
    public CommandResultCallback callback() {
        return callback;
    }

    @Override
    public Collection<String> getOnlinePlayerNames() {
        return List.of(server.getPlayerNames());
    }

    @Override
    public Collection<String> getAllTeams() {
        return org.bukkit.Bukkit.getServer().getScoreboardManager().getMainScoreboard().getTeams().stream()
            .map(org.bukkit.scoreboard.Team::getName).toList();
    }

    @Override
    public Stream<Identifier> getAvailableSounds() {
        return Stream.empty();
    }

    @Override
    public Stream<Identifier> getAvailablePostEffects() {
        return Stream.empty();
    }

    @Override
    public CompletableFuture<Suggestions> customSuggestion(CommandContext<?> context) {
        return Suggestions.empty();
    }

    @Override
    public <E> CompletableFuture<Suggestions> suggestRegistryElements(ResourceKey<? extends Registry<E>> key,
        SharedSuggestionProvider.ElementSuggestionType elements, SuggestionsBuilder builder, CommandContext<?> context,
        Predicate<E> filter) {
        return registryAccess().<E>lookup(key).map(registry -> {
            suggestRegistryElements(registry, elements, builder, filter);
            return builder.buildFuture();
        }).orElseGet(Suggestions::empty);
    }

    @Override
    public Set<ResourceKey<Level>> levels() {
        return server.levelKeys();
    }

    @Override
    public RegistryAccess registryAccess() {
        return server.registryAccess();
    }

    @Override
    public FeatureFlagSet enabledFeatures() {
        return FeatureFlagSet.of();
    }

    @Override
    public CommandDispatcher<CommandSourceStack> dispatcher() {
        return dispatcher != null ? dispatcher : server.getCommands().getDispatcher();
    }

    public CommandSourceStack withDispatcher(CommandDispatcher<CommandSourceStack> dispatcher) {
        CommandSourceStack stack = copy(source, position, rotation, level, permissions, entity, silent, callback, anchor,
            signingContext, chatMessageChainer);
        stack.dispatcher = dispatcher;
        return stack;
    }

    @Override
    public void handleError(CommandExceptionType type, Message message, boolean forked, TraceCallbacks tracer) {
        if (tracer != null) {
            tracer.onError(message.getString());
        }
        if (!forked) {
            sendFailure(Component.literal(message.getString()));
        }
    }

    @Override
    public boolean isSilent() {
        return silent;
    }

    public interface NamesProvider {
        NamesProvider FOR_ENTITY = new NamesProvider() {
            @Override
            public Component displayName(Entity entity) {
                return entity.getDisplayName();
            }

            @Override
            public String textName(Entity entity) {
                return entity.getName().getString();
            }
        };

        Component displayName(Entity entity);

        String textName(Entity entity);

        static NamesProvider constant(Component value) {
            return new NamesProvider() {
                @Override
                public Component displayName(Entity entity) {
                    return value;
                }

                @Override
                public String textName(Entity entity) {
                    return value.getString();
                }
            };
        }
    }
}
