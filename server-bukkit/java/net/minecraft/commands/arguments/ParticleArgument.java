// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import com.mojang.serialization.DataResult;
import java.util.Arrays;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.core.particles.ParticleType;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.TagParser;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
public class ParticleArgument implements ArgumentType<ParticleOptions> {
    private static final Collection<String> EXAMPLES = Arrays.asList("foo", "foo:bar", "particle{foo:bar}");
    public static final DynamicCommandExceptionType ERROR_UNKNOWN_PARTICLE = new DynamicCommandExceptionType(value -> Component.translatableEscape("particle.notFound", value));
    public static final DynamicCommandExceptionType ERROR_INVALID_OPTIONS = new DynamicCommandExceptionType(message -> Component.translatableEscape("particle.invalidOptions", message));
    private static final TagParser<?> kValueParser = TagParser.create(NbtOps.INSTANCE);
    private final HolderLookup.Provider registries;
    public ParticleArgument(CommandBuildContext context) {
        this.registries = context;
    }
    public static ParticleArgument particle(CommandBuildContext context) {
        return new ParticleArgument(context);
    }
    public static ParticleOptions getParticle(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, ParticleOptions.class);
    }
    public ParticleOptions parse(StringReader reader) throws CommandSyntaxException {
        return readParticle(reader, registries);
    }
    public Collection<String> getExamples() {
        return EXAMPLES;
    }
    public static ParticleOptions readParticle(StringReader reader, HolderLookup.Provider registries) throws CommandSyntaxException {
        Identifier id = Identifier.read(reader);
        ParticleType<?> type = BuiltInRegistries.PARTICLE_TYPE.get(id).orElseThrow(() -> ERROR_UNKNOWN_PARTICLE.createWithContext(reader, id)).value();
        return readOptions(kValueParser, reader, type, registries);
    }
    private static <T extends ParticleOptions, O> T readOptions(TagParser<O> parser, StringReader reader, ParticleType<T> type, HolderLookup.Provider registries) throws CommandSyntaxException {
        var ops = registries.createSerializationContext(parser.getOps());
        O extraData = reader.canRead() && reader.peek() == '{' ? parser.parseAsArgument(reader) : ops.emptyMap();
        DataResult<T> parsed = type.codec().codec().parse(ops, extraData);
        return parsed.getOrThrow(ERROR_INVALID_OPTIONS::create);
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        String remaining = builder.getRemaining();
        for (Identifier id : BuiltInRegistries.PARTICLE_TYPE.keySet()) {
            if (id.toString().startsWith(remaining) || id.getPath().startsWith(remaining)) {
                builder.suggest(id.toString());
            }
        }
        return builder.buildFuture();
    }
}
