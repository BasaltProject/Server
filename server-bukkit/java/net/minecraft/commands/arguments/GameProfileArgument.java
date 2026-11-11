// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.network.chat.Component;
import java.util.List;
import net.minecraft.commands.SharedSuggestionProvider;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.permissions.Permissions;
import net.minecraft.commands.arguments.selector.EntitySelectorParser;
import net.minecraft.commands.arguments.selector.EntitySelector;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.arguments.selector.EntitySelector;
import net.minecraft.server.players.NameAndId;
public class GameProfileArgument implements ArgumentType<GameProfileArgument.Result> {
    public static final SimpleCommandExceptionType ERROR_UNKNOWN_PLAYER = new SimpleCommandExceptionType(Component.translatable("argument.player.unknown"));
    private static final Collection<String> EXAMPLES = java.util.List.of("Player", "0123", "dd12be42-52a9-4a91-a8a1-11c01849e498", "@e");

    private static Result read(StringReader reader, boolean allowSelectors) throws CommandSyntaxException {
        if (reader.canRead() && reader.peek() == '@') {
            EntitySelector selector = new EntitySelectorParser(reader, allowSelectors).parse();
            if (selector.includesEntities()) {
                throw EntityArgument.ERROR_ONLY_PLAYERS_ALLOWED.createWithContext(reader);
            }
            return new SelectorResult(selector);
        }
        int start = reader.getCursor();
        while (reader.canRead() && reader.peek() != ' ') {
            reader.skip();
        }
        String name = reader.getString().substring(start, reader.getCursor());
        return source -> java.util.Collections.singleton(source.getServer().services().nameToIdCache().get(name).orElseThrow(ERROR_UNKNOWN_PLAYER::create));
    }
    public static Collection<NameAndId> getGameProfiles(CommandContext<CommandSourceStack> source, String name) throws CommandSyntaxException {
        return source.getArgument(name, Result.class).getNames(source.getSource());
    }
    public static GameProfileArgument gameProfile() {
        return new GameProfileArgument();
    }
    public <S> Result parse(StringReader reader, S source) throws CommandSyntaxException {
        return read(reader, EntitySelectorParser.allowSelectors(source));
    }
    public Result parse(StringReader reader) throws CommandSyntaxException {
        return read(reader, true);
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> contextBuilder, SuggestionsBuilder builder) {
        if (!(contextBuilder.getSource() instanceof SharedSuggestionProvider source)) {
            return Suggestions.empty();
        }
        StringReader reader = new StringReader(builder.getInput());
        reader.setCursor(builder.getStart());
        EntitySelectorParser parser = new EntitySelectorParser(reader, source.permissions().hasPermission(Permissions.COMMANDS_ENTITY_SELECTORS));
        try {
            parser.parse();
        } catch (CommandSyntaxException ignored) {
            // suggestions are built from whatever was parsed so far
        }
        return parser.fillSuggestions(builder, names -> SharedSuggestionProvider.suggest(source.getOnlinePlayerNames(), names));
    }
    public Collection<String> getExamples() {
        return EXAMPLES;
    }
    public static class SelectorResult implements Result {
        private final EntitySelector selector;

        public SelectorResult(EntitySelector selector) {
            this.selector = selector;
            
        }
        public Collection<NameAndId> getNames(CommandSourceStack sender) throws CommandSyntaxException {
            List<ServerPlayer> players = selector.findPlayers(sender);
            if (players.isEmpty()) {
                throw EntityArgument.NO_PLAYERS_FOUND.create();
            }
            return players.stream().map(ServerPlayer::nameAndId).toList();
        }
    }
    public interface Result {
        Collection<NameAndId> getNames(CommandSourceStack sender) throws CommandSyntaxException;
    }
}
