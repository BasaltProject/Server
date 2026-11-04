// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.contents;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.datafixers.DataFixUtils;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import java.util.stream.Stream;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.arguments.NbtPathArgument;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.nbt.TextComponentTagVisitor;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentContents;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.chat.ComponentUtils;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.chat.ResolutionContext;
import net.minecraft.network.chat.contents.data.DataSource;
import net.minecraft.network.chat.contents.data.DataSources;
import net.minecraft.util.CompilableString;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public record NbtContents(CompilableString<NbtPathArgument.NbtPath> nbtPath, boolean interpreting, boolean plain, Optional<Component> separator, DataSource dataSource) implements ComponentContents {
    private static final Logger LOGGER = LoggerFactory.getLogger(NbtContents.class);
    public static final Codec<CompilableString<NbtPathArgument.NbtPath>> NBT_PATH_CODEC = CompilableString.codec(new CompilableString.CommandParserHelper<NbtPathArgument.NbtPath>() {
        protected NbtPathArgument.NbtPath parse(StringReader reader) throws CommandSyntaxException {
            return new NbtPathArgument().parse(reader);
        }
        protected String errorMessage(String original, CommandSyntaxException exception) {
            return "Invalid NBT path: " + original + ": " + exception.getMessage();
        }
    });
    public static final MapCodec<NbtContents> MAP_CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
        NBT_PATH_CODEC.fieldOf("nbt").forGetter(NbtContents::nbtPath),
        Codec.BOOL.lenientOptionalFieldOf("interpret", false).forGetter(NbtContents::interpreting),
        Codec.BOOL.lenientOptionalFieldOf("plain", false).forGetter(NbtContents::plain),
        ComponentSerialization.CODEC.lenientOptionalFieldOf("separator").forGetter(NbtContents::separator),
        DataSources.CODEC.forGetter(NbtContents::dataSource)).apply(i, NbtContents::new));
    public MutableComponent resolve(ResolutionContext context, int recursionDepth) throws CommandSyntaxException {
        CommandSourceStack source = context.source();
        if (source == null || context.resolvedComponentCount().intValue() > context.resolvedComponentLimit()) {
            return Component.empty();
        }
        context.resolvedComponentCount().increment();
        Stream<Tag> elements = dataSource.getData(source).flatMap(root -> {
            try {
                return nbtPath.compiled().get(root).stream();
            } catch (CommandSyntaxException failure) {
                return Stream.empty();
            }
        });
        Component resolvedSeparator = DataFixUtils.orElse(ComponentUtils.resolve(context, separator, recursionDepth), ComponentUtils.DEFAULT_NO_STYLE_SEPARATOR);
        if (interpreting) {
            var ops = source.registryAccess().createSerializationContext(NbtOps.INSTANCE);
            return elements.flatMap(tag -> {
                try {
                    Component parsed = ComponentSerialization.CODEC.parse(ops, tag).getOrThrow();
                    return Stream.of(ComponentUtils.resolve(context, parsed, recursionDepth));
                } catch (Exception failure) {
                    LOGGER.warn("Failed to parse component: {}", tag, failure);
                    return Stream.<MutableComponent>of();
                }
            }).reduce((left, right) -> left.append(resolvedSeparator).append((Component) right)).orElseGet(Component::empty);
        }
        return elements.map(tag -> {
            var visitor = new TextComponentTagVisitor("", plain ? TextComponentTagVisitor.PlainStyling.INSTANCE : TextComponentTagVisitor.RichStyling.INSTANCE);
            return (MutableComponent) visitor.visit(tag);
        }).reduce((left, right) -> left.append(resolvedSeparator).append((Component) right)).orElseGet(Component::empty);
    }
    public MapCodec<NbtContents> codec() {
        return MAP_CODEC;
    }
}
