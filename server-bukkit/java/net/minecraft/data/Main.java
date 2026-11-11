// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data;

import java.io.IOException;
import java.nio.file.Path;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.data.info.BiomeParametersDumpReport;
import net.minecraft.data.info.BlockListReport;
import net.minecraft.data.info.CommandsReport;
import net.minecraft.data.info.DatapackStructureReport;
import net.minecraft.data.info.PacketReport;
import net.minecraft.data.info.RegistryComponentsReport;
import net.minecraft.data.info.RegistryDumpReport;
import net.minecraft.data.registries.RegistriesDatapackGenerator;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.data.structures.NbtToSnbt;
import net.minecraft.data.structures.SnbtToNbt;
import net.minecraft.data.structures.StructureUpdater;

public class Main {
    public static void main(String[] args) throws IOException {
        Path output = Path.of("generated");
        boolean server = false;
        boolean reports = false;
        boolean dev = false;
        java.util.List<Path> inputs = new java.util.ArrayList<>();
        for (String arg : args) {
            if (arg.startsWith("--output=")) {
                output = Path.of(arg.substring("--output=".length()));
            } else if (arg.startsWith("--input=")) {
                inputs.add(Path.of(arg.substring("--input=".length())));
            } else if (arg.equals("--server")) {
                server = true;
            } else if (arg.equals("--reports")) {
                reports = true;
            } else if (arg.equals("--dev")) {
                dev = true;
            } else if (arg.equals("--all")) {
                server = true;
                reports = true;
            }
        }
        DataGenerator generator = new DataGenerator.Uncached(output);
        addServerConverters(generator, inputs, server, dev);
        addServerDefinitionProviders(generator, server, reports);
        generator.run();
    }

    public static void addServerConverters(DataGenerator generator, Collection<Path> input, boolean server, boolean dev) {
        DataGenerator.PackGenerator pack = generator.getVanillaPack(server);
        pack.addProvider(output -> new SnbtToNbt(output, input).addFilter(new StructureUpdater()));
        DataGenerator.PackGenerator snbt = generator.getVanillaPack(dev);
        snbt.addProvider(output -> new NbtToSnbt(output, input));
    }

    public static void addServerDefinitionProviders(DataGenerator generator, boolean server, boolean reports) {
        CompletableFuture<HolderLookup.Provider> lookup = CompletableFuture.completedFuture(VanillaRegistries.createWorldLookup());
        DataGenerator.PackGenerator pack = generator.getVanillaPack(server);
        pack.addProvider(output -> RegistriesDatapackGenerator.forWorldLayer(output, lookup));
        pack.addProvider(output -> RegistriesDatapackGenerator.forReloadableLayer(output, lookup));
        pack.addProvider(output -> new NativeFolderProvider(output, "Recipes", "recipe"));
        pack.addProvider(output -> new NativeFolderProvider(output, "Advancements", "advancement"));
        pack.addProvider(output -> new NativeFolderProvider(output, "Loot Tables", "loot_table"));
        pack.addProvider(output -> new net.minecraft.data.tags.BiomeTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.DamageTypeTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.EntityTypeTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.FlatLevelGeneratorPresetTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.InstrumentTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.FluidTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.VanillaBlockTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.GameEventTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.PoiTypeTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.BannerPatternTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.FeatureTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.PotionTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.TimelineTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.PaintingVariantTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.DialogTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.VanillaItemTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.VanillaEnchantmentTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.StructureTagsProvider(output, lookup));
        pack.addProvider(output -> new net.minecraft.data.tags.WorldPresetTagsProvider(output, lookup));
        DataGenerator.PackGenerator trade = generator.getBuiltinDatapack(server, "trade_rebalance");
        trade.addProvider(output -> new net.minecraft.data.tags.TradeRebalanceTradeTagsProvider(output, lookup));
        trade.addProvider(output -> new net.minecraft.data.tags.TradeRebalanceEnchantmentTagsProvider(output, lookup));
        trade.addProvider(output -> new net.minecraft.data.tags.VillagerTradesTagsProvider(output, lookup));
        DataGenerator.PackGenerator info = generator.getVanillaPack(reports);
        info.addProvider(output -> new BiomeParametersDumpReport(output, lookup));
        info.addProvider(output -> new BlockListReport(output, lookup));
        info.addProvider(output -> new CommandsReport(output, lookup));
        info.addProvider(output -> new RegistryComponentsReport(output, lookup));
        info.addProvider(RegistryDumpReport::new);
        info.addProvider(PacketReport::new);
        info.addProvider(DatapackStructureReport::new);
    }
}
