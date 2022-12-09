
export function tsTypeFromIdl(ty) {
    switch (ty) {
        case "bool":
            return "bool"
        case "u8":
            return "uint8_t"
        case "i8":
            return "int8_t"
        case "u16":
            return "uint16_t"
        case "i16":
            return "int16_t"
        case "u32":
            return "uint32_t"
        case "i32":
            return "int32_t"
        case "f32":
            return "float"
        case "u64":
            return "uint64_t"
        case "i64":
            return "int64_t"
        case "f64":
            return "double"
        case "u128":
            return "uint128_t"
        case "i128":
            return "int128_t"
        case "bytes":
            return "std::vector<int> Array"
        case "string":
            return "std::string"
        case "publicKey":
            return "PublicKey"
        default:
            if ("vec" in ty) {
                return `std::vector<int>`
            }
            if ("option" in ty) {
                return `${tsTypeFromIdl(
                    ty.option,
                )}`
            }
            if ("coption" in ty) {
                return `${tsTypeFromIdl(
                    ty.coption,
                )}`
            }
            if ("array" in ty) {
                return `std::vector<int>`
            }
            else {
                return "std::string"
            }
    }
}