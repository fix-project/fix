use std::{convert::TryFrom, fmt::Display};

use anyhow::{bail, ensure, Context, Result};

/// Number of bytes
const METADATA_LENGTH: usize = 1;
/// 64 bit number => 8 bytes
const UINT64_LENGTH: usize = 8;
/// 256 bits => 32 bytes
const HANDLE_LENGTH: usize = 32;
const LITERAL_CONTENT_LENGTH: usize = HANDLE_LENGTH - METADATA_LENGTH;
const CANONICAL_HASH_LENGTH: usize = HANDLE_LENGTH - UINT64_LENGTH - METADATA_LENGTH;

#[derive(Debug, PartialEq, serde::Deserialize, serde::Serialize, Clone)]
pub(crate) struct Task {
    pub(crate) handle: Handle,
    pub(crate) operation: Operation,
}

#[derive(Debug, PartialEq, serde::Deserialize, serde::Serialize, Clone, Copy)]
pub(crate) enum Operation {
    Apply,
    Eval,
    Fill,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, serde::Deserialize, serde::Serialize)]
pub(crate) struct Handle {
    pub(crate) size: u64,
    pub(crate) accessibility: Accessibility,
    pub(crate) content: Content,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, serde::Deserialize, serde::Serialize)]
pub(crate) enum Content {
    Other {
        object_type: Object,
        data: Nonliteral,
    },
    Literal([u8; LITERAL_CONTENT_LENGTH]),
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, serde::Deserialize, serde::Serialize)]
pub(crate) enum Nonliteral {
    Canonical([u8; CANONICAL_HASH_LENGTH]),
    Local(u64),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, serde::Deserialize, serde::Serialize)]
pub(crate) enum Accessibility {
    Strict,
    Shallow,
    Lazy,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, serde::Deserialize, serde::Serialize)]
pub(crate) enum Object {
    Blob,
    Tree,
    Thunk,
    Tag,
}

impl Handle {
    /// Parses a handle in format [unsigned 64 bit number as hex]-[u64 as hex]-[u64 as hex]-[u64 as hex].
    /// For example, d9-0-4-0 or 10-0-0-2400000000000000.
    /// Also takes d9|0|4|0 for compatibility with fixpoint handle formatting.
    pub(crate) fn from_hex(input: &str) -> Result<Self> {
        let handle_content = input
            .split(|c| c == '-' || c == '|')
            .map(|i| u64::from_str_radix(i, 16))
            .collect::<Result<Vec<_>, _>>()
            .context("Failed to parse handle from hex")?;
        ensure!(
            handle_content.len() == 4,
            "Expected handle with exactly 4 parts when parsing from hex"
        );
        let handle_content: [u64; 4] = handle_content.try_into().unwrap();
        let handle_content: [u8; HANDLE_LENGTH] = handle_content
            .into_iter()
            .flat_map(|i| i.to_le_bytes().into_iter())
            .collect::<Vec<_>>()
            .try_into()
            .unwrap();
        // metadata is
        // if handle is literal:
        //     | strict/shallow/lazy (2 bits) | 1 (1 bit) | size of blob (5 bits)
        // if not a literal:
        //     | strict/shallow/lazy (2 bits) | 0 (1 bit) | 00 | canonical/local (1 bit) | Blob/Tree/Thunk/Tag (2 bits)
        let metadata: u8 = handle_content[HANDLE_LENGTH - 1];
        let is_literal = metadata & 0b10_0000 != 0;
        let accessibility =
            Accessibility::try_from(metadata >> 6).context("getting accessibility bits")?;
        if is_literal {
            // Handle structure
            // data (8 bytes) | data (8 bytes) | data (8 bytes) | data (7 bytes) | metadata (1 byte)
            let literal_size = metadata & 0b1_1111;
            let mut content = [0u8; LITERAL_CONTENT_LENGTH];
            content.copy_from_slice(&handle_content[..LITERAL_CONTENT_LENGTH]);
            return Ok(Self {
                size: literal_size.into(),
                accessibility,
                content: Content::Literal(content),
            });
        }
        let is_canonical = metadata & 0b100 != 0;
        let object_type = Object::try_from(metadata & 0b11).context("getting object bits")?;
        let size = u64::from_le_bytes(
            handle_content[UINT64_LENGTH * 2..UINT64_LENGTH * 3]
                .try_into()
                .unwrap(),
        );
        if is_canonical {
            // Handle structure
            // hash (8 bytes) | hash (8 bytes) | size (8 bytes) | hash (7 bytes) | metadata (1 byte)
            let mut hash = [0u8; CANONICAL_HASH_LENGTH];
            hash[..UINT64_LENGTH * 2].copy_from_slice(&handle_content[..UINT64_LENGTH * 2]);
            hash[UINT64_LENGTH * 2..]
                .copy_from_slice(&handle_content[UINT64_LENGTH * 3..HANDLE_LENGTH - 1]);
            return Ok(Self {
                size,
                accessibility,
                content: Content::Other {
                    object_type,
                    data: Nonliteral::Canonical(hash),
                },
            });
        }

        let local_id = u64::from_le_bytes(handle_content[..UINT64_LENGTH].try_into().unwrap());
        Ok(Self {
            size,
            accessibility,
            content: Content::Other {
                object_type,
                data: Nonliteral::Local(local_id),
            },
        })
    }

    /// Reconstructs the hex string version of a Handle
    pub(crate) fn to_hex(&self) -> String {
        self.to_buffer()
            .chunks_exact(UINT64_LENGTH)
            .map(|s: &[u8]| format!("{:x}", u64::from_le_bytes(s.try_into().unwrap())))
            .collect::<Vec<_>>()
            .join("-")
    }

    fn to_buffer(&self) -> [u8; HANDLE_LENGTH] {
        let mut out_content = [0_u8; HANDLE_LENGTH];

        out_content[HANDLE_LENGTH - 1] |= Into::<u8>::into(self.accessibility) << 6;

        let data = match &self.content {
            Content::Literal(data) => {
                out_content[..LITERAL_CONTENT_LENGTH].copy_from_slice(data);
                out_content[HANDLE_LENGTH - 1] |= 0b10_0000;
                out_content[HANDLE_LENGTH - 1] |= (self.size & 0xFF) as u8;
                return out_content;
            }
            Content::Other { object_type, data } => {
                out_content[HANDLE_LENGTH - 1] |= Into::<u8>::into(*object_type);
                data
            }
        };

        out_content[UINT64_LENGTH * 2..UINT64_LENGTH * 3].copy_from_slice(&self.size.to_le_bytes());

        match data {
            Nonliteral::Canonical(hash) => {
                out_content[HANDLE_LENGTH - 1] |= 0b100;
                out_content[..UINT64_LENGTH * 2].copy_from_slice(&hash[..UINT64_LENGTH * 2]);
                out_content[UINT64_LENGTH * 3..HANDLE_LENGTH - 1]
                    .copy_from_slice(&hash[UINT64_LENGTH * 2..]);
            }
            Nonliteral::Local(id) => {
                out_content[..UINT64_LENGTH].copy_from_slice(&id.to_le_bytes());
            }
        }

        out_content
    }
}

impl TryFrom<u8> for Operation {
    type Error = anyhow::Error;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::Apply,
            1 => Self::Eval,
            2 => Self::Fill,
            _ => bail!("Invalid number for Operation"),
        })
    }
}

impl From<Operation> for u8 {
    fn from(val: Operation) -> Self {
        match val {
            Operation::Apply => 0,
            Operation::Eval => 1,
            Operation::Fill => 2,
        }
    }
}

impl Display for Operation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Operation::Apply => "Apply",
            Operation::Eval => "Eval",
            Operation::Fill => "Fill",
        })
    }
}

impl Operation {
    pub fn get_color(&self) -> egui::Color32 {
        match self {
            Operation::Apply => egui::Color32::GREEN,
            Operation::Eval => egui::Color32::from_rgb(20, 20, 255),
            Operation::Fill => egui::Color32::RED,
        }
    }
}

impl Display for Handle {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        // strict Tree, 3 entries, local id d9
        // shallow Thunk, 8 entries, canonical id 0x0123456
        // strict Blob, 8 bytes, content "unused"
        // strict Blob, 2 bytes, content 0x91ABCD
        // strict Blob, 2 bytes, local id ab
        let accessibility = match self.accessibility {
            Accessibility::Strict => "strict",
            Accessibility::Shallow => "shallow",
            Accessibility::Lazy => "lazy",
        };
        let content_type = match self.content {
            Content::Other { object_type, .. } => object_type,
            Content::Literal(_) => Object::Blob,
        };
        let size = self.size;
        let size_desc = match content_type {
            Object::Blob => "bytes",
            _ => "entries",
        };

        let identifier = match &self.content {
            Content::Other { data, .. } => match data {
                Nonliteral::Canonical(hash) => {
                    // 8 hex digits of hash
                    format!(
                        "canonical id 0x{:0>8}",
                        hash.iter()
                            .take(4)
                            .map(|byte| format!("{:x}", byte))
                            .collect::<String>()
                    )
                }
                Nonliteral::Local(id) => format!("local id 0x{:x}", id),
            },
            Content::Literal(content) => {
                let valid_content = &content[..self.size as usize];
                let mut base = format!(
                    "content {}",
                    // Try to parse as string. If success, return the string.
                    if let (true, Ok(string)) = (
                        valid_content.iter().all(|c| !c.is_ascii_control()),
                        String::from_utf8(valid_content.to_vec())
                    ) {
                        format!("\"{}\"", string)
                    } else {
                        // Otherwise, return a hex string.
                        format!(
                            "0x{}",
                            valid_content
                                .iter()
                                .map(|byte| format!("{:0>2x}", byte))
                                .collect::<String>()
                        )
                    }
                );

                fn try_append<A, I: Display, S: TryInto<A>>(
                    slice: S,
                    array_to_integer: impl Fn(A) -> I,
                    result: &mut String,
                ) {
                    if let Ok(array) = slice.try_into() {
                        result.push_str(&format!(
                            " ({}: {})",
                            std::any::type_name::<I>(),
                            array_to_integer(array)
                        ))
                    }
                }

                try_append(valid_content, u8::from_le_bytes, &mut base);
                try_append(valid_content, u16::from_le_bytes, &mut base);
                try_append(valid_content, u32::from_le_bytes, &mut base);
                try_append(valid_content, u64::from_le_bytes, &mut base);

                base
            }
        };
        f.write_fmt(format_args!(
            "{accessibility} {content_type}, {size} {size_desc}, {identifier}"
        ))
    }
}

impl TryFrom<u8> for Accessibility {
    type Error = anyhow::Error;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::Strict,
            1 => Self::Shallow,
            2 => Self::Lazy,
            _ => bail!("Invalid number for Accessibility"),
        })
    }
}

impl From<Accessibility> for u8 {
    fn from(val: Accessibility) -> Self {
        match val {
            Accessibility::Strict => 0,
            Accessibility::Shallow => 1,
            Accessibility::Lazy => 2,
        }
    }
}

impl TryFrom<u8> for Object {
    type Error = anyhow::Error;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::Tree,
            1 => Self::Thunk,
            2 => Self::Blob,
            3 => Self::Tag,
            _ => bail!("Invalid number for Object"),
        })
    }
}

impl From<Object> for u8 {
    fn from(val: Object) -> Self {
        match val {
            Object::Tree => 0,
            Object::Thunk => 1,
            Object::Blob => 2,
            Object::Tag => 3,
        }
    }
}

impl Display for Object {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Object::Blob => "Blob",
            Object::Tree => "Tree",
            Object::Thunk => "Thunk",
            Object::Tag => "Tag",
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn blob() {
        // &strict Blob(10|0|0|2400000000000000)
        let mut content = [0; LITERAL_CONTENT_LENGTH];
        content[0] = 16;
        let handle_string = "10-0-0-2400000000000000";
        let handle = Handle {
            // created as a u32
            size: 4,
            accessibility: Accessibility::Strict,
            content: Content::Literal(content),
        };
        assert_eq!(Handle::from_hex(handle_string).unwrap(), handle);
        assert_eq!(handle_string, handle.to_hex());
    }

    #[test]
    fn thunk() {
        // &strict Thunk(d9|0|4|100000000000000)
        let handle_string = "d9-0-4-100000000000000";
        let handle = Handle {
            // 4 entries
            size: 4,
            accessibility: Accessibility::Strict,
            content: Content::Other {
                object_type: Object::Thunk,
                data: Nonliteral::Local(217),
            },
        };
        assert_eq!(Handle::from_hex(handle_string).unwrap(), handle);
        assert_eq!(handle_string, handle.to_hex());
    }

    #[test]
    fn tag() {
        // &strict Tag(862fcba5ecaade2c|4b24159ac7c28a29|3|715eb1e41f37d42)
        let handle_string = "862fcba5ecaade2c-4b24159ac7c28a29-3-715eb1e41f37d42";
        let mut content = [0; CANONICAL_HASH_LENGTH];
        content[..UINT64_LENGTH].copy_from_slice(
            &u64::from_str_radix("862fcba5ecaade2c", 16)
                .unwrap()
                .to_le_bytes(),
        );
        content[UINT64_LENGTH..UINT64_LENGTH * 2].copy_from_slice(
            &u64::from_str_radix("4b24159ac7c28a29", 16)
                .unwrap()
                .to_le_bytes(),
        );
        // zero out the last byte because that is metadata
        // and is not part of the content hash
        content[UINT64_LENGTH * 2..].copy_from_slice(
            &u64::from_str_radix("0015eb1e41f37d42", 16)
                .unwrap()
                .to_le_bytes()[0..UINT64_LENGTH - 1],
        );
        let handle = Handle {
            // all tags have 3 entries
            size: 3,
            accessibility: Accessibility::Strict,
            content: Content::Other {
                object_type: Object::Tag,
                data: Nonliteral::Canonical(content),
            },
        };
        assert_eq!(Handle::from_hex(handle_string).unwrap(), handle);
        assert_eq!(handle_string, handle.to_hex());
    }
}
