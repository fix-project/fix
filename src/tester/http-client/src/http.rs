use std::sync::{mpsc::Sender, Arc};

use anyhow::{anyhow, Context, Result};
use reqwest::Client;
use serde::de::DeserializeOwned;

use crate::{app::Relation, handle::Handle};

pub(crate) fn get<T, S, F>(
    client: Arc<Client>,
    ctx: egui::Context,
    url: String,
    map: F,
    tx: Sender<Result<S>>,
) where
    T: DeserializeOwned + Send,
    S: Send + 'static,
    F: FnOnce(T) -> Result<S> + Send + 'static,
{
    let task = async move {
        let result = client.get(url).send().await;
        match result {
            Ok(ok) => {
                let json = ok.json::<T>().await;
                let _ = tx.send(json.context("parsing json").and_then(map));
            }
            Err(e) => {
                let _ = tx.send(Err(anyhow::anyhow!(format!(
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
        ctx.request_repaint();
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

pub(crate) fn get_explanations(
    client: Arc<Client>,
    ctx: egui::Context,
    handle: &Handle,
    tx: Sender<Result<Vec<Relation>>>,
    url_base: &str,
) {
    #[derive(serde::Deserialize)]
    struct JsonResponse {
        target: String,
        relations: EmptyStringOrVec<JsonRelation>,
        handles: EmptyStringOrVec<Handle>,
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

    get(
        client,
        ctx,
        format!("http://{url_base}/explanations?handle={}", handle.to_hex()),
        |json: JsonResponse| {
            let mut results = vec![];
            if let EmptyStringOrVec::Vec(_handles) = json.handles {
                todo!();
            }
            if let EmptyStringOrVec::Vec(relations) = json.relations {
                for relation in relations {
                    let result = Relation {
                        lhs: Handle::from_hex(&relation.lhs)
                            .with_context(|| format!("parsing {}", relation.lhs))?,
                        rhs: Handle::from_hex(&relation.rhs)
                            .with_context(|| format!("parsing {}", relation.rhs))?,
                        relation_type: match relation.relation.as_str() {
                            "Apply" => crate::app::RelationType::Apply,
                            "Eval" => crate::app::RelationType::Eval,
                            "Fill" => crate::app::RelationType::Fill,
                            _ => return Err(anyhow!("invalid relation {}", relation.relation)),
                        },
                    };
                    results.push(result)
                }
            }

            Ok(results)
        },
        tx,
    );
}
