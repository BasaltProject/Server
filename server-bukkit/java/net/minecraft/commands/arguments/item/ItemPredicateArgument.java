// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.item;
import java.util.Map;
import java.util.Objects;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.serialization.Codec;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.core.HolderSet;
import net.minecraft.core.Registry;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.RegistryOps;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.util.Unit;
import net.minecraft.util.Util;
import net.minecraft.world.item.Item;
import com.mojang.brigadier.ImmutableStringReader;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.serialization.Decoder;
import com.mojang.serialization.Dynamic;
import java.util.Collection;
import java.util.List;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.predicates.DataComponentPredicate;
import net.minecraft.resources.Identifier;
import net.minecraft.util.parsing.packrat.commands.ParserBasedArgument;
import net.minecraft.world.item.ItemStack;
public class ItemPredicateArgument extends ParserBasedArgument<ItemPredicateArgument.Result> {
    private static final Collection<String> kExamples = List.of("stick", "minecraft:stick", "#stick", "#stick{foo:'bar'}");
    private static final DynamicCommandExceptionType kUnknownItem = new DynamicCommandExceptionType(id -> Component.translatableEscape("argument.item.id.invalid", id));
    private static final DynamicCommandExceptionType kUnknownTag = new DynamicCommandExceptionType(tag -> Component.translatableEscape("arguments.item.tag.unknown", tag));
    private static final DynamicCommandExceptionType kUnknownComponent = new DynamicCommandExceptionType(id -> Component.translatableEscape("arguments.item.component.unknown", id));
    private static final Dynamic2CommandExceptionType kMalformedComponent = new Dynamic2CommandExceptionType((type, message) -> Component.translatableEscape("arguments.item.component.malformed", type, message));
    private static final DynamicCommandExceptionType kUnknownPredicate = new DynamicCommandExceptionType(id -> Component.translatableEscape("arguments.item.predicate.unknown", id));
    private static final Dynamic2CommandExceptionType kMalformedPredicate = new Dynamic2CommandExceptionType((type, message) -> Component.translatableEscape("arguments.item.predicate.malformed", type, message));
    private static final Identifier kCount = Identifier.withDefaultNamespace("count");
    private static final Map<Identifier, ComponentWrapper> kPseudoComponents = Map.of(kCount,
        new ComponentWrapper(kCount, stack -> true, MinMaxBounds.Ints.CODEC.map(range -> stack -> range.matches(stack.getCount()))));
    private static final Map<Identifier, PredicateWrapper> kPseudoPredicates = Map.of(kCount,
        new PredicateWrapper(kCount, MinMaxBounds.Ints.CODEC.map(range -> stack -> range.matches(stack.getCount()))));

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static ResourceKey<DataComponentType<?>> componentKey(Identifier id) {
        return ResourceKey.create((ResourceKey<? extends Registry<DataComponentType<?>>>) (ResourceKey) Registries.DATA_COMPONENT_TYPE, id);
    }

    private static PredicateWrapper existencePredicate(Holder.Reference<DataComponentType<?>> component) {
        Predicate<ItemStack> present = stack -> stack.has(component.value());
        return new PredicateWrapper(component.key().identifier(), Unit.CODEC.map(unit -> present));
    }

    public ItemPredicateArgument(CommandBuildContext registries) {
        super(ComponentPredicateParser.createGrammar(new Context(registries)).mapResult(predicates -> Util.allOf(predicates)::test));
    }
    public static ItemPredicateArgument itemPredicate(CommandBuildContext context) {
        return new ItemPredicateArgument(context);
    }
    public static Result getItemPredicate(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Result.class);
    }
    public Collection<String> getExamples() {
        return kExamples;
    }
    static record ComponentWrapper(Identifier id, Predicate<ItemStack> presenceChecker, Decoder<? extends Predicate<ItemStack>> valueChecker) {
        public static <T> ComponentWrapper create(ImmutableStringReader reader, Identifier id, DataComponentType<T> type) throws CommandSyntaxException {
            Codec<T> codec = type.codec();
            if (codec == null) {
                throw kUnknownComponent.createWithContext(reader, id);
            }
            return new ComponentWrapper(id, stack -> stack.has(type), codec.map(expected -> stack -> Objects.equals(expected, stack.get(type))));
        }
        public Predicate<ItemStack> decode(ImmutableStringReader reader, Dynamic<?> value) throws CommandSyntaxException {
            return valueChecker.parse(value).getOrThrow(message -> kMalformedComponent.createWithContext(reader, id.toString(), message));
        }
    }
    static record PredicateWrapper(Identifier id, Decoder<? extends Predicate<ItemStack>> type) {
        public PredicateWrapper(Holder.Reference<DataComponentPredicate.Type<?>> holder) {
            this(holder.key().identifier(), holder.value().codec().map(predicate -> predicate::matches));
        }
        public Predicate<ItemStack> decode(ImmutableStringReader reader, Dynamic<?> value) throws CommandSyntaxException {
            return type.parse(value).getOrThrow(message -> kMalformedPredicate.createWithContext(reader, id.toString(), message));
        }
    }
    static class Context implements ComponentPredicateParser.Context<Predicate<ItemStack>, ComponentWrapper, PredicateWrapper> {
        private final HolderLookup.Provider registries;
        private final HolderLookup.RegistryLookup<Item> items;
        private final HolderLookup.RegistryLookup<DataComponentType<?>> components;
        private final HolderLookup.RegistryLookup<DataComponentPredicate.Type<?>> predicates;

        private Context(HolderLookup.Provider registries) {
            this.registries = registries;
            items = registries.lookupOrThrow(Registries.ITEM);
            components = (HolderLookup.RegistryLookup<DataComponentType<?>>) (HolderLookup.RegistryLookup) registries.lookupOrThrow((ResourceKey) Registries.DATA_COMPONENT_TYPE);
            predicates = registries.lookupOrThrow(Registries.DATA_COMPONENT_PREDICATE_TYPE);
            
        }
        public Predicate<ItemStack> forElementType(ImmutableStringReader reader, Identifier id) throws CommandSyntaxException {
            Holder.Reference<Item> item = items.get(ResourceKey.create(Registries.ITEM, id)).orElseThrow(() -> kUnknownItem.createWithContext(reader, id));
            return stack -> stack.is(item);
        }
        public Predicate<ItemStack> forTagType(ImmutableStringReader reader, Identifier id) throws CommandSyntaxException {
            HolderSet<Item> tag = items.get(TagKey.create(Registries.ITEM, id)).orElseThrow(() -> kUnknownTag.createWithContext(reader, id));
            return stack -> stack.is(tag);
        }
        public ComponentWrapper lookupComponentType(ImmutableStringReader reader, Identifier componentId) throws CommandSyntaxException {
            ComponentWrapper pseudo = kPseudoComponents.get(componentId);
            if (pseudo != null) {
                return pseudo;
            }
            DataComponentType<?> type = components.get(componentKey(componentId)).map(Holder::value)
                .orElseThrow(() -> kUnknownComponent.createWithContext(reader, componentId));
            return ComponentWrapper.create(reader, componentId, type);
        }
        public Predicate<ItemStack> createComponentTest(ImmutableStringReader reader, ComponentWrapper componentType, Dynamic<?> value) throws CommandSyntaxException {
            return componentType.decode(reader, RegistryOps.injectRegistryContext(value, registries));
        }
        public Predicate<ItemStack> createComponentTest(ImmutableStringReader reader, ComponentWrapper componentType) {
            return componentType.presenceChecker();
        }
        public PredicateWrapper lookupPredicateType(ImmutableStringReader reader, Identifier componentId) throws CommandSyntaxException {
            PredicateWrapper pseudo = kPseudoPredicates.get(componentId);
            if (pseudo != null) {
                return pseudo;
            }
            return predicates.get(ResourceKey.create(Registries.DATA_COMPONENT_PREDICATE_TYPE, componentId)).map(PredicateWrapper::new)
                .or(() -> components.get(componentKey(componentId)).map(ItemPredicateArgument::existencePredicate))
                .orElseThrow(() -> kUnknownPredicate.createWithContext(reader, componentId));
        }
        public Predicate<ItemStack> createPredicateTest(ImmutableStringReader reader, PredicateWrapper predicateType, Dynamic<?> value) throws CommandSyntaxException {
            return predicateType.decode(reader, RegistryOps.injectRegistryContext(value, registries));
        }
        public Stream<Identifier> listElementTypes() {
            return items.listElementIds().map(ResourceKey::identifier);
        }
        public Stream<Identifier> listTagTypes() {
            return items.listTagIds().map(TagKey::location);
        }
        public Stream<Identifier> listComponentTypes() {
            return Stream.concat(kPseudoComponents.keySet().stream(),
                components.listElements().filter(entry -> !entry.value().isTransient()).map(entry -> entry.key().identifier()));
        }
        public Stream<Identifier> listPredicateTypes() {
            return Stream.concat(kPseudoPredicates.keySet().stream(), predicates.listElementIds().map(ResourceKey::identifier));
        }
        public Predicate<ItemStack> negate(Predicate<ItemStack> value) {
            return value.negate();
        }
        public Predicate<ItemStack> anyOf(List<Predicate<ItemStack>> alternatives) {
            return Util.anyOf(alternatives);
        }
    }
    public interface Result extends Predicate<ItemStack> {
    }
}
