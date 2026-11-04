// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.contents;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.commands.arguments.selector.EntitySelector;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentContents;
import net.minecraft.network.chat.FormattedText;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.chat.ResolutionContext;
import net.minecraft.network.chat.Style;
import net.minecraft.util.CompilableString;
public record SelectorContents(CompilableString<EntitySelector> selector, Optional<Component> separator) implements ComponentContents {
    public static final MapCodec<SelectorContents> MAP_CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(instance -> instance.group(
        CompilableString.codec(text -> com.mojang.serialization.DataResult.success(EntitySelector.ofText(text, EntitySelector.INFINITE, true))).fieldOf("selector").forGetter(SelectorContents::selector),
        net.minecraft.network.chat.ComponentSerialization.CODEC.optionalFieldOf("separator").forGetter(SelectorContents::separator)).apply(instance, SelectorContents::new));
    public MapCodec<SelectorContents> codec() {
        return MAP_CODEC;
    }
    public MutableComponent resolve(ResolutionContext context, int recursionDepth) throws CommandSyntaxException {
        java.util.List<Component> names = new java.util.ArrayList<>();
        for (net.minecraft.world.entity.Entity entity : selector.compiled().findEntities(context.source())) {
            names.add(entity.getDisplayName());
        }
        return net.minecraft.network.chat.ComponentUtils.formatList(names, separator.orElse(net.minecraft.network.chat.ComponentUtils.DEFAULT_SEPARATOR), name -> name);
    }
    public <T> Optional<T> visit(FormattedText.StyledContentConsumer<T> output, Style currentStyle) {
        return output.accept(currentStyle, selector.source());
    }
    public <T> Optional<T> visit(FormattedText.ContentConsumer<T> output) {
        return output.accept(selector.source());
    }
}
