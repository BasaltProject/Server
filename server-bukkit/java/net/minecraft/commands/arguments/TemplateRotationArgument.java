// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import com.mojang.brigadier.context.CommandContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.world.level.block.Rotation;
public class TemplateRotationArgument extends StringRepresentableArgument<Rotation> {
    private TemplateRotationArgument() {
        super(null, Rotation::values);
    }
    public static TemplateRotationArgument templateRotation() {
        return new TemplateRotationArgument();
    }
    public static Rotation getRotation(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Rotation.class);
    }
}
