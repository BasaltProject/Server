// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.network.chat.Component;
public class CommandResponseTracker<Element> {
    private int total;
    private final Object[] sole = new Object[2];
    private final int[] seen = new int[2];

    private void note(int slot, Element element) {
        seen[slot]++;
        sole[slot] = seen[slot] == 1 ? element : null;
    }

    @SuppressWarnings("unchecked")
    private Element soleOf(ElementType type) {
        return (Element) sole[type.ordinal()];
    }

    private int countOf(ElementType type) {
        return seen[type.ordinal()];
    }

    public static <T> CommandResponseTracker<T> create() {
        return new CommandResponseTracker<>();
    }
    public void track(Element element, int value) {
        total += value;
        note(0, element);
        if (value != 0) {
            note(1, element);
        }
    }
    public void track(Element element, boolean value) {
        track(element, value ? 1 : 0);
    }
    public void track(Element element) {
        track(element, 1);
    }
    public int totalValue() {
        return total;
    }
    public static <Element> Messages<Element> messages(SingleHandler<Component, Element> onSingle, MultipleHandler<Component> onMultiple) {
        return new Messages<>(null, new Dispatch<>(onSingle, onMultiple));
    }
    public static <Element> Messages<Element> messages(ErrorHandler onZero, SingleHandler<Component, Element> onSingle, MultipleHandler<Component> onMultiple) {
        return new Messages<>(onZero, new Dispatch<>(onSingle, onMultiple));
    }
    public static <Element> Messages<Element> messages(SimpleCommandExceptionType onZero, SingleHandler<Component, Element> onSingle, MultipleHandler<Component> onMultiple) {
        return new Messages<>(onZero::create, new Dispatch<>(onSingle, onMultiple));
    }
    public <Result> Result dispatch(ElementType elementType, Dispatch<Result, ? super Element> dispatch) {
        Element only = soleOf(elementType);
        return only != null ? dispatch.onSingle().create(only, total) : dispatch.onMultiple().create(countOf(elementType), total);
    }
    public int sendFeedback(CommandSourceStack sourceStack, boolean broadcast, ElementType elementType, Messages<? super Element> messages) throws CommandSyntaxException {
        messages.throwIfZero(countOf(elementType));
        sourceStack.sendSuccess(() -> dispatch(elementType, messages.onSuccess()), broadcast);
        return total;
    }
    public int sendFeedback(CommandSourceStack sourceStack, boolean broadcast, Messages<? super Element> messages) throws CommandSyntaxException {
        return sendFeedback(sourceStack, broadcast, ElementType.NON_ZERO, messages);
    }
    public static <Element, Arg> MessagesWithArg<Element, Arg> messages(SingleHandlerWithArg<Component, Element, Arg> onSingle, MultipleHandlerWithArg<Component, Arg> onMultiple) {
        return new MessagesWithArg<>(null, new DispatchWithArg<>(onSingle, onMultiple));
    }
    public static <Element, Arg> MessagesWithArg<Element, Arg> messages(ErrorHandlerWithArg<Arg> onZero, SingleHandlerWithArg<Component, Element, Arg> onSingle, MultipleHandlerWithArg<Component, Arg> onMultiple) {
        return new MessagesWithArg<>(onZero, new DispatchWithArg<>(onSingle, onMultiple));
    }
    public static <Element, Arg> MessagesWithArg<Element, Arg> messages(SimpleCommandExceptionType onZero, SingleHandlerWithArg<Component, Element, Arg> onSingle, MultipleHandlerWithArg<Component, Arg> onMultiple) {
        return new MessagesWithArg<>(argument -> onZero.create(), new DispatchWithArg<>(onSingle, onMultiple));
    }
    public <Result, Arg> Result dispatch(ElementType elementType, DispatchWithArg<Result, ? super Element, Arg> dispatch, Arg argument) {
        Element only = soleOf(elementType);
        return only != null ? dispatch.onSingle().create(only, total, argument)
            : dispatch.onMultiple().create(countOf(elementType), total, argument);
    }
    public <Arg> int sendFeedback(CommandSourceStack sourceStack, boolean broadcast, ElementType elementType, MessagesWithArg<? super Element, Arg> messages, Arg argument) throws CommandSyntaxException {
        messages.throwIfZero(countOf(elementType), argument);
        sourceStack.sendSuccess(() -> dispatch(elementType, messages.onSuccess(), argument), broadcast);
        return total;
    }
    public <Arg> int sendFeedback(CommandSourceStack sourceStack, boolean broadcast, MessagesWithArg<? super Element, Arg> messages, Arg argument) throws CommandSyntaxException {
        return sendFeedback(sourceStack, broadcast, ElementType.NON_ZERO, messages, argument);
    }
    public static <Element, Arg0, Arg1> MessagesWithArgs<Element, Arg0, Arg1> messages(SingleHandlerWithArgs<Component, Element, Arg0, Arg1> onSingle, MultipleHandlerWithArgs<Component, Arg0, Arg1> onMultiple) {
        return new MessagesWithArgs<>(null, new DispatchWithArgs<>(onSingle, onMultiple));
    }
    public static <Element, Arg0, Arg1> MessagesWithArgs<Element, Arg0, Arg1> messages(ErrorHandlerWithArgs<Arg0, Arg1> onZero, SingleHandlerWithArgs<Component, Element, Arg0, Arg1> onSingle, MultipleHandlerWithArgs<Component, Arg0, Arg1> onMultiple) {
        return new MessagesWithArgs<>(onZero, new DispatchWithArgs<>(onSingle, onMultiple));
    }
    public static <Element, Arg0, Arg1> MessagesWithArgs<Element, Arg0, Arg1> messages(SimpleCommandExceptionType onZero, SingleHandlerWithArgs<Component, Element, Arg0, Arg1> onSingle, MultipleHandlerWithArgs<Component, Arg0, Arg1> onMultiple) {
        return new MessagesWithArgs<>((first, second) -> onZero.create(), new DispatchWithArgs<>(onSingle, onMultiple));
    }
    public <Result, Arg0, Arg1> Result dispatch(ElementType elementType, DispatchWithArgs<Result, ? super Element, Arg0, Arg1> messages, Arg0 argument0, Arg1 argument1) {
        Element only = soleOf(elementType);
        return only != null ? messages.onSingle().create(only, total, argument0, argument1)
            : messages.onMultiple().create(countOf(elementType), total, argument0, argument1);
    }
    public <Arg0, Arg1> int sendFeedback(CommandSourceStack sourceStack, boolean broadcast, ElementType elementType, MessagesWithArgs<? super Element, Arg0, Arg1> messages, Arg0 argument0, Arg1 argument1) throws CommandSyntaxException {
        messages.throwIfZero(countOf(elementType), argument0, argument1);
        sourceStack.sendSuccess(() -> dispatch(elementType, messages.onSuccess(), argument0, argument1), broadcast);
        return total;
    }
    public <Arg0, Arg1> int sendFeedback(CommandSourceStack sourceStack, boolean broadcast, MessagesWithArgs<? super Element, Arg0, Arg1> messages, Arg0 argument0, Arg1 argument1) throws CommandSyntaxException {
        return sendFeedback(sourceStack, broadcast, ElementType.NON_ZERO, messages, argument0, argument1);
    }
    public static enum ElementType {
        ANY, NON_ZERO;
    }
    public static record Dispatch<Result, Element>(SingleHandler<Result, Element> onSingle, MultipleHandler<Result> onMultiple) {
    }
    public static record Messages<Element>(ErrorHandler onZero, Dispatch<Component, Element> onSuccess) {
        public void throwIfZero(int value) throws CommandSyntaxException {
            if (value == 0 && onZero != null) {
                throw onZero.get();
            }
        }
    }
    public static record DispatchWithArg<Result, Element, Arg>(SingleHandlerWithArg<Result, Element, Arg> onSingle, MultipleHandlerWithArg<Result, Arg> onMultiple) {
    }
    public static record MessagesWithArg<Element, Arg>(ErrorHandlerWithArg<Arg> onZero, DispatchWithArg<Component, Element, Arg> onSuccess) {
        public void throwIfZero(int value, Arg argument) throws CommandSyntaxException {
            if (value == 0 && onZero != null) {
                throw onZero.get(argument);
            }
        }
    }
    public static record DispatchWithArgs<Result, Element, Arg0, Arg1>(SingleHandlerWithArgs<Result, Element, Arg0, Arg1> onSingle, MultipleHandlerWithArgs<Result, Arg0, Arg1> onMultiple) {
    }
    public static record MessagesWithArgs<Element, Arg0, Arg1>(ErrorHandlerWithArgs<Arg0, Arg1> onZero, DispatchWithArgs<Component, Element, Arg0, Arg1> onSuccess) {
        public void throwIfZero(int value, Arg0 argument0, Arg1 argument1) throws CommandSyntaxException {
            if (value == 0 && onZero != null) {
                throw onZero.get(argument0, argument1);
            }
        }
    }
    public interface ErrorHandler {
        CommandSyntaxException get();
    }
    public interface ErrorHandlerWithArg<Arg> {
        CommandSyntaxException get(Arg value);
    }
    public interface ErrorHandlerWithArgs<Arg0, Arg1> {
        CommandSyntaxException get(Arg0 first, Arg1 second);
    }
    public interface MultipleHandler<Result> {
        Result create(int count, int total);
    }
    public interface MultipleHandlerWithArg<Result, Arg> {
        Result create(int count, int total, Arg value);
    }
    public interface MultipleHandlerWithArgs<Result, Arg0, Arg1> {
        Result create(int count, int total, Arg0 first, Arg1 second);
    }
    public interface SingleHandler<Result, Element> {
        Result create(Element element, int total);
    }
    public interface SingleHandlerWithArg<Result, Element, Arg> {
        Result create(Element element, int total, Arg value);
    }
    public interface SingleHandlerWithArgs<Result, Element, Arg0, Arg1> {
        Result create(Element element, int total, Arg0 first, Arg1 second);
    }
}
