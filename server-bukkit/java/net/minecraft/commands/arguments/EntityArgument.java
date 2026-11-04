// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.server.MinecraftServer;
import com.google.gson.JsonObject;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.arguments.selector.EntitySelector;
import net.minecraft.commands.synchronization.ArgumentTypeInfo;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
public class EntityArgument implements ArgumentType<EntitySelector> {
    public static final SimpleCommandExceptionType ERROR_NOT_SINGLE_ENTITY = new SimpleCommandExceptionType(new com.mojang.brigadier.LiteralMessage("Only one entity is allowed, but the provided selector allows more than one"));
    public static final SimpleCommandExceptionType ERROR_NOT_SINGLE_PLAYER = new SimpleCommandExceptionType(new com.mojang.brigadier.LiteralMessage("Only one player is allowed, but the provided selector allows more than one"));
    public static final SimpleCommandExceptionType ERROR_ONLY_PLAYERS_ALLOWED = new SimpleCommandExceptionType(new com.mojang.brigadier.LiteralMessage("Only players may be affected by this command, but the provided selector includes entities"));
    public static final SimpleCommandExceptionType NO_ENTITIES_FOUND = new SimpleCommandExceptionType(new com.mojang.brigadier.LiteralMessage("No entity was found"));
    public static final SimpleCommandExceptionType NO_PLAYERS_FOUND = new SimpleCommandExceptionType(new com.mojang.brigadier.LiteralMessage("No player was found"));
    public static final SimpleCommandExceptionType ERROR_SELECTORS_NOT_ALLOWED = new SimpleCommandExceptionType(new com.mojang.brigadier.LiteralMessage("Selectors are not allowed"));
    private boolean single;
    private boolean playersOnly;
    protected EntityArgument(boolean single, boolean playersOnly) {
        this.single = single;
        this.playersOnly = playersOnly;
        
    }
    public static EntityArgument entity() {
        return new EntityArgument(true, false);
    }
    public static Entity getEntity(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return context.getArgument(name, EntitySelector.class).findSingleEntity(context.getSource());
    }
    public static EntityArgument entities() {
        return new EntityArgument(false, false);
    }
    public static Collection<? extends Entity> getEntities(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        Collection<? extends Entity> found = getOptionalEntities(context, name);
        if (found.isEmpty()) {
            throw NO_ENTITIES_FOUND.create();
        }
        return found;
    }
    public static Collection<? extends Entity> getOptionalEntities(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return context.getArgument(name, EntitySelector.class).findEntities(context.getSource());
    }
    public static Collection<ServerPlayer> getOptionalPlayers(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return context.getArgument(name, EntitySelector.class).findPlayers(context.getSource());
    }
    public static EntityArgument player() {
        return new EntityArgument(true, true);
    }
    public static ServerPlayer getPlayer(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return context.getArgument(name, EntitySelector.class).findSinglePlayer(context.getSource());
    }
    public static EntityArgument players() {
        return new EntityArgument(false, true);
    }
    public static Collection<ServerPlayer> getPlayers(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        Collection<ServerPlayer> found = getOptionalPlayers(context, name);
        if (found.isEmpty()) {
            throw NO_PLAYERS_FOUND.create();
        }
        return found;
    }
    public EntitySelector parse(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        int depth = 0;
        while (reader.canRead() && (depth > 0 || reader.peek() != ' ')) {
            char c = reader.peek();
            if (c == '[') {
        depth++;
            } else if (c == ']') {
        depth--;
            }
            reader.skip();
        }
        String text = reader.getString().substring(start, reader.getCursor());
        if (text.isEmpty()) {
            throw NO_ENTITIES_FOUND.createWithContext(reader);
        }
        EntitySelector selector = EntitySelector.ofText(text, single ? 1 : EntitySelector.INFINITE, !playersOnly);
        if (playersOnly && selector.usesSelector() && !text.startsWith("@a") && !text.startsWith("@p") && !text.startsWith("@r") && !text.startsWith("@s")) {
            reader.setCursor(start);
            throw ERROR_ONLY_PLAYERS_ALLOWED.createWithContext(reader);
        }
        return selector;
    }
    public <S> EntitySelector parse(StringReader reader, S source) throws CommandSyntaxException {
        return parse(reader);
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> contextBuilder, SuggestionsBuilder builder) {
        String remaining = builder.getRemaining();
        for (String selector : java.util.List.of("@a", "@e", "@p", "@r", "@s")) {
            if (selector.startsWith(remaining)) {
        builder.suggest(selector);
            }
        }
        for (String player : MinecraftServer.getServer().getPlayerNames()) {
            if (player.toLowerCase(java.util.Locale.ROOT).startsWith(remaining.toLowerCase(java.util.Locale.ROOT))) {
        builder.suggest(player);
            }
        }
        return builder.buildFuture();
    }
    public Collection<String> getExamples() {
        return java.util.List.of("Player", "0123", "@e", "@e[type=foo]", "dd12be42-52a9-4a91-a8a1-11c01849e498");
    }
    public static class Info implements ArgumentTypeInfo<EntityArgument, Info.Template> {
        private static final int kSingle = 1;
        private static final int kPlayersOnly = 2;

        public void serializeToNetwork(Template template, FriendlyByteBuf out) {
            out.writeByte((template.single ? kSingle : 0) | (template.playersOnly ? kPlayersOnly : 0));
        }

        public Template deserializeFromNetwork(FriendlyByteBuf in) {
            byte flags = in.readByte();
            return new Template((flags & kSingle) != 0, (flags & kPlayersOnly) != 0);
        }

        public void serializeToJson(Template template, JsonObject out) {
            out.addProperty("amount", template.single ? "single" : "multiple");
            out.addProperty("type", template.playersOnly ? "players" : "entities");
        }

        public Template unpack(EntityArgument argument) {
            return new Template(argument.single, argument.playersOnly);
        }

        public final class Template implements ArgumentTypeInfo.Template<EntityArgument> {
            final boolean single;
            final boolean playersOnly;

            private Template(boolean single, boolean playersOnly) {
                this.single = single;
                this.playersOnly = playersOnly;
            }

            public EntityArgument instantiate(CommandBuildContext context) {
                return new EntityArgument(single, playersOnly);
            }

            public ArgumentTypeInfo<EntityArgument, ?> type() {
                return Info.this;
            }
        }
    }

}
