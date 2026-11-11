// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.decoration.painting;
import java.util.Optional;
import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.Component;
import java.util.List;
import java.util.Set;
import java.util.Optional;
import net.minecraft.resources.Identifier;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
public class PaintingVariants {
    public static final ResourceKey<PaintingVariant> KEBAB = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("kebab"));
    public static final ResourceKey<PaintingVariant> AZTEC = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("aztec"));
    public static final ResourceKey<PaintingVariant> ALBAN = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("alban"));
    public static final ResourceKey<PaintingVariant> AZTEC2 = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("aztec2"));
    public static final ResourceKey<PaintingVariant> BOMB = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("bomb"));
    public static final ResourceKey<PaintingVariant> PLANT = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("plant"));
    public static final ResourceKey<PaintingVariant> WASTELAND = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("wasteland"));
    public static final ResourceKey<PaintingVariant> POOL = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("pool"));
    public static final ResourceKey<PaintingVariant> COURBET = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("courbet"));
    public static final ResourceKey<PaintingVariant> SEA = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("sea"));
    public static final ResourceKey<PaintingVariant> SUNSET = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("sunset"));
    public static final ResourceKey<PaintingVariant> CREEBET = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("creebet"));
    public static final ResourceKey<PaintingVariant> WANDERER = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("wanderer"));
    public static final ResourceKey<PaintingVariant> GRAHAM = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("graham"));
    public static final ResourceKey<PaintingVariant> MATCH = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("match"));
    public static final ResourceKey<PaintingVariant> BUST = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("bust"));
    public static final ResourceKey<PaintingVariant> STAGE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("stage"));
    public static final ResourceKey<PaintingVariant> VOID = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("void"));
    public static final ResourceKey<PaintingVariant> SKULL_AND_ROSES = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("skull_and_roses"));
    public static final ResourceKey<PaintingVariant> WITHER = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("wither"));
    public static final ResourceKey<PaintingVariant> FIGHTERS = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("fighters"));
    public static final ResourceKey<PaintingVariant> POINTER = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("pointer"));
    public static final ResourceKey<PaintingVariant> PIGSCENE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("pigscene"));
    public static final ResourceKey<PaintingVariant> BURNING_SKULL = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("burning_skull"));
    public static final ResourceKey<PaintingVariant> SKELETON = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("skeleton"));
    public static final ResourceKey<PaintingVariant> DONKEY_KONG = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("donkey_kong"));
    public static final ResourceKey<PaintingVariant> EARTH = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("earth"));
    public static final ResourceKey<PaintingVariant> WIND = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("wind"));
    public static final ResourceKey<PaintingVariant> WATER = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("water"));
    public static final ResourceKey<PaintingVariant> FIRE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("fire"));
    public static final ResourceKey<PaintingVariant> BAROQUE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("baroque"));
    public static final ResourceKey<PaintingVariant> HUMBLE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("humble"));
    public static final ResourceKey<PaintingVariant> MEDITATIVE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("meditative"));
    public static final ResourceKey<PaintingVariant> PRAIRIE_RIDE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("prairie_ride"));
    public static final ResourceKey<PaintingVariant> UNPACKED = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("unpacked"));
    public static final ResourceKey<PaintingVariant> BACKYARD = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("backyard"));
    public static final ResourceKey<PaintingVariant> BOUQUET = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("bouquet"));
    public static final ResourceKey<PaintingVariant> CAVEBIRD = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("cavebird"));
    public static final ResourceKey<PaintingVariant> CHANGING = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("changing"));
    public static final ResourceKey<PaintingVariant> COTAN = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("cotan"));
    public static final ResourceKey<PaintingVariant> ENDBOSS = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("endboss"));
    public static final ResourceKey<PaintingVariant> FERN = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("fern"));
    public static final ResourceKey<PaintingVariant> FINDING = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("finding"));
    public static final ResourceKey<PaintingVariant> LOWMIST = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("lowmist"));
    public static final ResourceKey<PaintingVariant> ORB = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("orb"));
    public static final ResourceKey<PaintingVariant> OWLEMONS = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("owlemons"));
    public static final ResourceKey<PaintingVariant> PASSAGE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("passage"));
    public static final ResourceKey<PaintingVariant> POND = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("pond"));
    public static final ResourceKey<PaintingVariant> SUNFLOWERS = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("sunflowers"));
    public static final ResourceKey<PaintingVariant> TIDES = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("tides"));
    public static final ResourceKey<PaintingVariant> DENNIS = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PAINTING_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("dennis"));
    public static void bootstrap(BootstrapContext<PaintingVariant> context) {
        int[][] sizes = {{1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {2, 1}, {2, 1}, {2, 1}, {2, 1}, {2, 1}, {1, 2}, {1, 2}, {2, 2}, {2, 2}, {2, 2},
            {2, 2}, {2, 2}, {2, 2}, {4, 2}, {4, 4}, {4, 4}, {4, 4}, {4, 3}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {4, 3}, {2, 2}, {2, 2}, {1, 1}, {1, 2}, {4, 4}, {3, 4},
            {3, 3}, {3, 3}, {4, 2}, {3, 3}, {3, 3}, {3, 3}, {4, 2}, {4, 2}, {4, 4}, {3, 3}, {4, 2}, {3, 4}, {3, 3}, {3, 3}, {3, 3}};
        List<ResourceKey<PaintingVariant>> keys = List.of(KEBAB, AZTEC, ALBAN, AZTEC2, BOMB, PLANT, WASTELAND, POOL, COURBET, SEA, SUNSET, CREEBET, WANDERER,
            GRAHAM, MATCH, BUST, STAGE, VOID, SKULL_AND_ROSES, WITHER, FIGHTERS, POINTER, PIGSCENE, BURNING_SKULL, SKELETON, EARTH, WIND, WATER, FIRE, DONKEY_KONG,
            BAROQUE, HUMBLE, MEDITATIVE, PRAIRIE_RIDE, UNPACKED, BACKYARD, BOUQUET, CAVEBIRD, CHANGING, COTAN, ENDBOSS, FERN, FINDING, LOWMIST, ORB, OWLEMONS,
            PASSAGE, POND, SUNFLOWERS, TIDES, DENNIS);
        Set<ResourceKey<PaintingVariant>> anonymous = Set.of(WITHER, EARTH, WIND, WATER, FIRE);
        for (int at = 0; at < keys.size(); at++) {
            ResourceKey<PaintingVariant> key = keys.get(at);
            Identifier id = key.identifier();
            Component title = Component.translatable(id.toLanguageKey("painting", "title")).withStyle(ChatFormatting.YELLOW);
            Optional<Component> author = anonymous.contains(key) ? Optional.empty()
                : Optional.of(Component.translatable(id.toLanguageKey("painting", "author")).withStyle(ChatFormatting.GRAY));
            context.register(key, new PaintingVariant(sizes[at][0], sizes[at][1], id, Optional.of(title), author));
        }

    }
}
