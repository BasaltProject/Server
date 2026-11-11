// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dialog.action;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import java.util.List;
import java.util.Map;
import net.minecraft.commands.functions.StringTemplate;
public class ParsedTemplate {
    public static final Codec<ParsedTemplate> CODEC = Codec.STRING.comapFlatMap(ParsedTemplate::parse, t -> t.raw);
    public static final Codec<String> VARIABLE_CODEC = Codec.STRING.validate(
            name -> StringTemplate.isValidVariableName(name) ? DataResult.success(name) : DataResult.error(() -> name + " is not a valid input name"));
    private final String raw;
    private final StringTemplate parsed;
    private ParsedTemplate(String raw, StringTemplate parsed) {
        this.raw = raw;
        this.parsed = parsed;
    }
    private static DataResult<ParsedTemplate> parse(String value) {
        try {
            return DataResult.success(new ParsedTemplate(value, StringTemplate.fromString(value)));
        } catch (Exception e) {
            return DataResult.error(() -> "Failed to parse template " + value + ": " + e.getMessage());
        }
    }
    public String instantiate(Map<String, String> arguments) {
        List<String> values = parsed.variables().stream().map(name -> arguments.getOrDefault(name, "")).toList();
        return parsed.substitute(values);
    }
}
