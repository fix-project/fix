use std::sync::{mpsc::Sender, Arc};

use anyhow::{anyhow, Context, Result};
use reqwest::Client;
use serde::de::DeserializeOwned;

use crate::{
    graphs::{Relation, RelationType},
    handle::{Handle, Object},
};

#[derive(Clone)]
pub(crate) struct HttpContext {
    pub(crate) client: Arc<Client>,
    pub(crate) egui_ctx: egui::Context,
    pub(crate) url_base: String,
    pub(crate) tx: Sender<Result<Vec<Relation>>>,
}

pub(crate) fn get<T, F>(ctx: HttpContext, map: F, url_path: String)
where
    T: DeserializeOwned + Send,
    F: FnOnce(T) -> Result<Vec<Relation>> + Send + 'static,
{
    let task = async move {
        let result = ctx
            .client
            .get(format!("http://{}{}", ctx.url_base, url_path))
            .send()
            .await;
        match result {
            Ok(ok) => {
                let json = ok.json::<T>().await;
                let _ = ctx.tx.send(json.context("parsing json").and_then(map));
            }
            Err(e) => {
                let _ = ctx.tx.send(Err(anyhow::anyhow!(format!(
                    "request failed: {} error",
                    match () {
                        () if e.is_builder() => "building url",
                        () if e.is_request() => "request",
                        () if e.is_redirect() => "redirect",
                        () if e.is_status() => "status code",
                        () if e.is_body() => "body",
                        () if e.is_decode() => "decode",
                        () if e.is_timeout() => "timeout",
                        () => "unknown",
                    }
                ))));
            }
        }
        ctx.egui_ctx.request_repaint();
    };
    #[cfg(target_arch = "wasm32")]
    wasm_bindgen_futures::spawn_local(task);
    #[cfg(not(target_arch = "wasm32"))]
    #[allow(clippy::let_underscore_future)]
    let _ = tokio::spawn(task);
}

#[derive(serde::Deserialize)]
struct JsonRelation {
    relation: String,
    lhs: String,
    rhs: String,
}

// The specific Boost for C++ being used only support property trees
// which serialize empty arrays as the empty string.
// Therefore, we catch the different type with this enum.
#[derive(serde::Deserialize)]
#[serde(untagged)]
enum EmptyStringOrVec<T> {
    String(String),
    Vec(Vec<T>),
}

fn parse_handle(handle: impl AsRef<str>) -> Result<Handle> {
    Handle::from_hex(handle.as_ref()).with_context(|| format!("parsing {}", handle.as_ref()))
}

pub(crate) fn get_explanations(ctx: HttpContext, handle: &Handle) {
    #[derive(serde::Deserialize)]
    struct JsonResponse {
        target: String,
        relations: EmptyStringOrVec<JsonRelation>,
        handles: EmptyStringOrVec<String>,
    }
    let handle_clone = handle.clone();

    get(
        ctx.clone(),
        move |json: JsonResponse| {
            let mut results = vec![];
            if let EmptyStringOrVec::Vec(handles) = json.handles {
                let handles = handles
                    .iter()
                    .map(parse_handle)
                    .collect::<Result<Vec<_>>>()?;
                get_pins_and_tags(ctx.clone(), &handle_clone, handles)
            }
            if let EmptyStringOrVec::Vec(relations) = json.relations {
                for relation in relations {
                    let result = Relation {
                        lhs: parse_handle(&relation.lhs)?,
                        rhs: parse_handle(&relation.rhs)?,
                        relation_type: match relation.relation.as_str() {
                            "Apply" => RelationType::Apply,
                            "Eval" => RelationType::Eval,
                            "Fill" => RelationType::Fill,
                            _ => return Err(anyhow!("invalid relation {}", relation.relation)),
                        },
                    };
                    results.push(result)
                }
            }

            Ok(results)
        },
        format!("/explanations?handle={}", handle.to_hex()),
    );
}

pub(crate) fn get_pins_and_tags(ctx: HttpContext, target: &Handle, pins_and_tags: Vec<Handle>) {
    // For every handle, if it is a tag, then fetch it.
    // If the target handle is in the the first slot of the tag, then
    // we use add a tag relation from the tag to the target.
    // Otherwise, we add a pin relation from the handle to the target.

    for handle in pins_and_tags {
        let target = target.clone();
        if handle.get_content_type() == Object::Tag {
            get_tag_contents(ctx.clone(), &handle.clone(), Some(target.clone()));
        } else {
            let _ = ctx.tx.send(Ok(vec![Relation::new(
                target.clone(),
                handle,
                RelationType::Pin,
            )]));
        }
    }
}

pub(crate) fn get_tag_contents(ctx: HttpContext, handle: &Handle, with_pin_target: Option<Handle>) {
    #[derive(serde::Deserialize)]
    struct JsonResponse {
        handles: EmptyStringOrVec<String>,
    }

    let handle_c = handle.clone();

    get(
        ctx.clone(),
        move |json: JsonResponse| {
            let EmptyStringOrVec::Vec(handles) = json.handles else {
                return Err(anyhow!("expected tag to have children"));
            };
            let handles = handles
                .into_iter()
                .map(parse_handle)
                .collect::<Result<Vec<_>>>()?;
            let handles: [Handle; 3] = handles
                .try_into()
                .map_err(|e: Vec<_>| anyhow!("found {} handles", e.len()))
                .context("expected tag to have three children")?;

            let mut result = vec![
                Relation::new(
                    handle_c.clone(),
                    handles[0].clone(),
                    RelationType::TagTarget,
                ),
                Relation::new(
                    handle_c.clone(),
                    handles[1].clone(),
                    RelationType::TagAuthor,
                ),
                Relation::new(handle_c.clone(), handles[2].clone(), RelationType::TagLabel),
            ];
            if let Some(target) = with_pin_target {
                result.push(Relation::new(handle_c.clone(), target, RelationType::Pin))
            }

            Ok(result)
        },
        format!("/tree_contents?handle={}", handle.to_hex()),
    )
}

pub(crate) fn get_tree_contents(ctx: HttpContext, handle: &Handle) {
    #[derive(serde::Deserialize)]
    struct JsonResponse {
        handles: EmptyStringOrVec<String>,
    }

    let handle_clone = handle.clone();

    get(
        ctx.clone(),
        move |json: JsonResponse| {
            let EmptyStringOrVec::Vec(entries) = json.handles else {
                return Ok(vec![]);
            };
            let entries = entries
                .into_iter()
                .map(parse_handle)
                .collect::<Result<Vec<_>>>()?;
            Ok(entries
                .into_iter()
                .enumerate()
                .map(|(i, e)| Relation::new(handle_clone.clone(), e, RelationType::TreeEntry(i)))
                .collect())
        },
        format!("/tree_contents?handle={}", handle.to_hex()),
    );
}
