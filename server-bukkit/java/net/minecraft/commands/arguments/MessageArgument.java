// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.Collection;
import java.util.function.Consumer;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.arguments.selector.EntitySelector;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.PlayerChatMessage;
public class MessageArgument implements SignedArgument<MessageArgument.Message> {
    public static MessageArgument message() {
        return new MessageArgument();
    }
    public static Component getMessage(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return context.getArgument(name, Message.class).toComponent(context.getSource(), true);
    }
    public static void resolveChatMessage(CommandContext<CommandSourceStack> context, String name, Consumer<PlayerChatMessage> task) throws CommandSyntaxException {
        Message message = context.getArgument(name, Message.class);
        Component component = message.toComponent(context.getSource(), true);
        task.accept(PlayerChatMessage.system(message.text()).withUnsignedContent(component));
    }
    public Message parse(StringReader reader) throws CommandSyntaxException {
        return Message.parseText(reader, true);
    }
    public <S> Message parse(StringReader reader, S source) throws CommandSyntaxException {
        return Message.parseText(reader, true);
    }
    public Collection<String> getExamples() {
        return java.util.List.of("Hello world!", "foo", "@e", "Hello @p :)");
    }
    public static record Message(String text, Part[] parts) {
        public Component toComponent(CommandSourceStack sender, boolean allowSelectors) throws CommandSyntaxException {
        return Component.literal(text);
        }
        public static Message parseText(StringReader reader, boolean allowSelectors) throws CommandSyntaxException {
        String text = reader.getRemaining();
        reader.setCursor(reader.getTotalLength());
        return new Message(text, new Part[0]);
        }
    }
    public static record Part(int start, int end, EntitySelector selector) {
        public Component toComponent(CommandSourceStack sender) throws CommandSyntaxException {
        return Component.literal(String.valueOf(selector));
        }
    }
}
