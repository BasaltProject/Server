// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.api;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.List;
import java.util.Optional;
import net.minecraft.resources.Identifier;

public record MethodInfo<Params, Result>(String description, Optional<ParamInfo<Params>> params, Optional<ResultInfo<Result>> result) {
    public MethodInfo(String description, ParamInfo<Params> paramInfo, ResultInfo<Result> resultInfo) {
        this(description, Optional.ofNullable(paramInfo), Optional.ofNullable(resultInfo));
    }

    private static <P> Codec<Optional<ParamInfo<P>>> paramsCodec() {
        return ParamInfo.<P>typedCodec().codec().listOf().xmap(
            list -> list.isEmpty() ? Optional.<ParamInfo<P>>empty() : Optional.of(list.get(0)),
            one -> one.map(List::of).orElseGet(List::of));
    }

    private static <P, R> MapCodec<MethodInfo<P, R>> typedCodec() {
        return RecordCodecBuilder.mapCodec(group -> group.group(
            Codec.STRING.fieldOf("description").forGetter(MethodInfo::description),
            MethodInfo.<P>paramsCodec().fieldOf("params").forGetter(MethodInfo::params),
            ResultInfo.<R>typedCodec().optionalFieldOf("result").forGetter(MethodInfo::result)).apply(group, MethodInfo::new));
    }

    public Named<Params, Result> named(Identifier name) {
        return new Named<>(name, this);
    }

    public static record Named<Params, Result>(Identifier name, MethodInfo<Params, Result> contents) {
        @SuppressWarnings({"unchecked", "rawtypes"})
        public static final Codec<Named<?, ?>> CODEC = (Codec) Named.<Object, Object>typedCodec();

        public static <Params, Result> Codec<Named<Params, Result>> typedCodec() {
            return RecordCodecBuilder.create(group -> group.group(
                Identifier.CODEC.fieldOf("name").forGetter(Named::name),
                MethodInfo.<Params, Result>typedCodec().forGetter(Named::contents)).apply(group, Named::new));
        }
    }
}
