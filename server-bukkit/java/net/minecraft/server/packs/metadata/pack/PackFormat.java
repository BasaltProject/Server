// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.metadata.pack;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.server.packs.PackType;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import java.util.function.BiFunction;
import net.minecraft.server.packs.PackType;
import net.minecraft.util.InclusiveRange;
public record PackFormat(int major, int minor) implements Comparable<PackFormat> {
    public static final Codec<PackFormat> BOTTOM_CODEC = fullCodec(0);

    private static Codec<PackFormat> fullCodec(int defaultMinor) {
        return ExtraCodecs.compactListCodec(ExtraCodecs.NON_NEGATIVE_INT, ExtraCodecs.NON_NEGATIVE_INT.listOf(1, 256)).xmap(
            list -> of(list.get(0), list.size() > 1 ? list.get(1) : defaultMinor),
            format -> format.minor != defaultMinor ? List.of(format.major(), format.minor()) : List.of(format.major()));
    }

    @Override
    public String toString() {
        return minor == Integer.MAX_VALUE ? major + ".*" : major + "." + minor;
    }
    public static final Codec<PackFormat> TOP_CODEC = fullCodec(Integer.MAX_VALUE);
    public static <ResultType, HolderType extends IntermediaryFormatHolder> DataResult<List<ResultType>> validateHolderList(List<HolderType> list, int lastPreMinorVersion, BiFunction<HolderType, InclusiveRange<PackFormat>, ResultType> constructor) {
        int earliest = list.stream().map(IntermediaryFormatHolder::format).mapToInt(IntermediaryFormat::effectiveMinMajorVersion).min().orElse(Integer.MAX_VALUE);
        List<ResultType> converted = new java.util.ArrayList<>(list.size());
        for (HolderType holder : list) {
            IntermediaryFormat format = holder.format();
            if (format.min().isEmpty() && format.max().isEmpty() && format.supported().isEmpty()) {
                continue;
            }
            DataResult<InclusiveRange<PackFormat>> range = format.validate(lastPreMinorVersion, false, earliest <= lastPreMinorVersion, "Overlay \"" + holder + "\"", "formats");
            if (range.isError()) {
                String message = range.error().get().message();
                return DataResult.error(() -> message);
            }
            converted.add(constructor.apply(holder, range.getOrThrow()));
        }
        return DataResult.success(List.copyOf(converted));
    }
    public static int lastPreMinorVersion(PackType type) {
        return type == PackType.CLIENT_RESOURCES ? 64 : 81;
    }
    public static MapCodec<InclusiveRange<PackFormat>> packCodec(PackType type) {
        int lastPre = lastPreMinorVersion(type);
        return IntermediaryFormat.PACK_CODEC.flatXmap(format -> format.validate(lastPre, true, false, "Pack", "supported_formats"),
            range -> DataResult.success(IntermediaryFormat.fromRange(range, lastPre)));
    }
    public static PackFormat of(int major, int minor) {
        return new PackFormat(major, minor);
    }
    public static PackFormat of(int major) {
        return new PackFormat(major, 0);
    }
    public InclusiveRange<PackFormat> minorRange() {
        return new InclusiveRange<>(this, of(major, Integer.MAX_VALUE));
    }
    public int compareTo(PackFormat other) {
        int byMajor = Integer.compare(major, other.major);
        return byMajor != 0 ? byMajor : Integer.compare(minor, other.minor);
    }
    public static record IntermediaryFormat(Optional<PackFormat> min, Optional<PackFormat> max, Optional<Integer> format, Optional<InclusiveRange<Integer>> supported) {
        private static final MapCodec<IntermediaryFormat> PACK_CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            PackFormat.BOTTOM_CODEC.optionalFieldOf("min_format").forGetter(IntermediaryFormat::min),
            PackFormat.TOP_CODEC.optionalFieldOf("max_format").forGetter(IntermediaryFormat::max),
            Codec.INT.optionalFieldOf("pack_format").forGetter(IntermediaryFormat::format),
            InclusiveRange.codec(Codec.INT).optionalFieldOf("supported_formats").forGetter(IntermediaryFormat::supported)).apply(i, IntermediaryFormat::new));

        private DataResult<InclusiveRange<PackFormat>> validateNew(int lastPre, boolean hasPackFormatField, boolean requireOldField, String context, String oldFieldName) {
            PackFormat low = min.get();
            PackFormat high = max.get();
            if (low.compareTo(high) > 0) {
                return DataResult.error(() -> context + " min_format (" + low + ") is greater than max_format (" + high + ")");
            }
            if (low.major() > lastPre && !requireOldField) {
                if (supported.isPresent()) {
                    return DataResult.error(() -> context + " key " + oldFieldName + " is deprecated starting from pack format " + (lastPre + 1) + ". Remove " + oldFieldName + " from your pack.mcmeta.");
                }
                String mismatch = hasPackFormatField && format.isPresent() ? checkMain(low.major(), high.major()) : null;
                if (mismatch != null) {
                    return DataResult.error(() -> mismatch);
                }
            } else {
                if (supported.isEmpty()) {
                    return DataResult.error(() -> context + " declares support for format " + low.major() + ", but game versions supporting formats 15 to " + lastPre + " require a " + oldFieldName + " field. Add \"" + oldFieldName + "\": [" + low.major() + ", " + lastPre + "] or require a version greater or equal to " + (lastPre + 1) + ".0.");
                }
                InclusiveRange<Integer> old = supported.get();
                if (old.minInclusive() != low.major()) {
                    return DataResult.error(() -> context + " version declaration mismatch between " + oldFieldName + " (from " + old.minInclusive() + ") and min_format (" + low + ")");
                }
                if (old.maxInclusive() != high.major() && old.maxInclusive() != lastPre) {
                    return DataResult.error(() -> context + " version declaration mismatch between " + oldFieldName + " (up to " + old.maxInclusive() + ") and max_format (" + high + ")");
                }
                if (hasPackFormatField) {
                    if (format.isEmpty()) {
                        return DataResult.error(() -> context + " declares support for formats up to " + lastPre + ", but game versions supporting formats 15 to " + lastPre + " require a pack_format field. Add \"pack_format\": " + low.major() + " or require a version greater or equal to " + (lastPre + 1) + ".0.");
                    }
                    String mismatch = checkMain(low.major(), high.major());
                    if (mismatch != null) {
                        return DataResult.error(() -> mismatch);
                    }
                }
            }
            return DataResult.success(new InclusiveRange<>(low, high));
        }

        private DataResult<InclusiveRange<PackFormat>> validateOld(int lastPre, boolean hasPackFormatField, String context, String oldFieldName) {
            int low = supported.get().minInclusive();
            int high = supported.get().maxInclusive();
            if (high > lastPre) {
                return DataResult.error(() -> context + " declares support for version newer than " + lastPre + ", but is missing mandatory fields min_format and max_format");
            }
            if (hasPackFormatField) {
                if (format.isEmpty()) {
                    return DataResult.error(() -> context + " declares support for formats up to " + lastPre + ", but game versions supporting formats 15 to " + lastPre + " require a pack_format field. Add \"pack_format\": " + low + " or require a version greater or equal to " + (lastPre + 1) + ".0.");
                }
                String mismatch = checkMain(low, high);
                if (mismatch != null) {
                    return DataResult.error(() -> mismatch);
                }
            }
            return DataResult.success(new InclusiveRange<>(low, high).map(PackFormat::of));
        }

        private String checkMain(int low, int high) {
            int main = format.get();
            if (main < low || main > high) {
                return "Pack declared support for versions " + low + " to " + high + " but declared main format is " + main;
            }
            return main < 15 ? "Multi-version packs cannot support minimum version of less than 15, since this will leave versions in range unable to load pack." : null;
        }

        public static final MapCodec<IntermediaryFormat> OVERLAY_CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            PackFormat.BOTTOM_CODEC.optionalFieldOf("min_format").forGetter(IntermediaryFormat::min),
            PackFormat.TOP_CODEC.optionalFieldOf("max_format").forGetter(IntermediaryFormat::max),
            InclusiveRange.codec(Codec.INT).optionalFieldOf("formats").forGetter(IntermediaryFormat::supported))
            .apply(i, (lo, hi, formats) -> new IntermediaryFormat(lo, hi, lo.map(PackFormat::major), formats)));
        public static IntermediaryFormat fromRange(InclusiveRange<PackFormat> range, int lastPreMinorVersion) {
            InclusiveRange<Integer> majors = range.map(PackFormat::major);
            boolean legacy = majors.isValueInRange(lastPreMinorVersion);
            return new IntermediaryFormat(Optional.of(range.minInclusive()), Optional.of(range.maxInclusive()),
                legacy ? Optional.of(majors.minInclusive()) : Optional.empty(), legacy ? Optional.of(new InclusiveRange<>(majors.minInclusive(), majors.maxInclusive())) : Optional.empty());
        }
        public int effectiveMinMajorVersion() {
            if (min.isPresent()) {
                return supported.isPresent() ? Math.min(min.get().major(), supported.get().minInclusive()) : min.get().major();
            }
            return supported.isPresent() ? supported.get().minInclusive() : Integer.MAX_VALUE;
        }
        public DataResult<InclusiveRange<PackFormat>> validate(int lastPreMinorVersion, boolean hasPackFormatField, boolean requireOldField, String context, String oldFieldName) {
            if (min.isPresent() != max.isPresent()) {
                return DataResult.error(() -> context + " missing field, must declare both min_format and max_format");
            }
            if (requireOldField && supported.isEmpty()) {
                return DataResult.error(() -> context + " missing required field " + oldFieldName + ", must be present in all overlays for any overlays to work across game versions");
            }
            if (min.isPresent()) {
                return validateNew(lastPreMinorVersion, hasPackFormatField, requireOldField, context, oldFieldName);
            }
            if (supported.isPresent()) {
                return validateOld(lastPreMinorVersion, hasPackFormatField, context, oldFieldName);
            }
            if (hasPackFormatField && format.isPresent()) {
                int single = format.get();
                return single > lastPreMinorVersion
                    ? DataResult.error(() -> context + " declares support for version newer than " + lastPreMinorVersion + ", but is missing mandatory fields min_format and max_format")
                    : DataResult.success(new InclusiveRange<>(PackFormat.of(single)));
            }
            return DataResult.error(() -> context + " could not be parsed, missing format version information");
        }
    }
    public interface IntermediaryFormatHolder {
        IntermediaryFormat format();
    }
}
