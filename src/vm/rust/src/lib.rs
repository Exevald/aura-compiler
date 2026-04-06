use std::collections::{HashMap, HashSet};
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;

pub struct SyncGraph {
    lock_graph: HashMap<String, HashSet<String>>,
    join_graph: HashMap<String, HashSet<String>>,
    last_error: CString,
}

impl SyncGraph {
    fn new() -> Self {
        Self {
            lock_graph: HashMap::new(),
            join_graph: HashMap::new(),
            last_error: CString::new("").unwrap(),
        }
    }

    fn set_error(&mut self, message: String) {
        self.last_error =
            CString::new(message).unwrap_or_else(|_| CString::new("sync analyzer error").unwrap());
    }

    fn clear_error(&mut self) {
        self.last_error = CString::new("").unwrap();
    }

    fn would_cycle(graph: &HashMap<String, HashSet<String>>, from: &str, to: &str) -> bool {
        if from == to {
            return true;
        }

        let mut visited = HashSet::new();
        let mut stack = vec![to.to_owned()];
        while let Some(current) = stack.pop() {
            if !visited.insert(current.clone()) {
                continue;
            }
            if current == from {
                return true;
            }
            if let Some(nexts) = graph.get(&current) {
                for next in nexts {
                    stack.push(next.clone());
                }
            }
        }
        false
    }

    fn add_edge(graph: &mut HashMap<String, HashSet<String>>, from: &str, to: &str) {
        graph
            .entry(from.to_owned())
            .or_default()
            .insert(to.to_owned());
    }

    fn add_lock_edge(&mut self, from: &str, to: &str, context: &str) -> bool {
        if Self::would_cycle(&self.lock_graph, from, to) {
            self.set_error(format!(
                "Potential deadlock detected at compile time: lock-order cycle involving '{}' -> '{}' in {}",
                from, to, context
            ));
            return false;
        }

        Self::add_edge(&mut self.lock_graph, from, to);
        self.clear_error();
        true
    }

    fn add_join_edge(&mut self, from: &str, to: &str, context: &str) -> bool {
        if Self::would_cycle(&self.join_graph, from, to) {
            self.set_error(format!(
                "Potential deadlock detected at compile time: join cycle involving '{}' -> '{}' in {}",
                from, to, context
            ));
            return false;
        }

        Self::add_edge(&mut self.join_graph, from, to);
        self.clear_error();
        true
    }
}

fn graph_mut<'a>(graph: *mut SyncGraph) -> Option<&'a mut SyncGraph> {
    unsafe { graph.as_mut() }
}

fn read_cstr(ptr: *const c_char) -> Option<String> {
    if ptr.is_null() {
        return None;
    }

    let cstr = unsafe { CStr::from_ptr(ptr) };
    Some(cstr.to_string_lossy().into_owned())
}

#[unsafe(no_mangle)]
pub extern "C" fn aura_sync_graph_new() -> *mut SyncGraph {
    Box::into_raw(Box::new(SyncGraph::new()))
}

#[unsafe(no_mangle)]
pub extern "C" fn aura_sync_graph_free(graph: *mut SyncGraph) {
    if graph.is_null() {
        return;
    }

    unsafe {
        drop(Box::from_raw(graph));
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn aura_sync_graph_add_lock_edge(
    graph: *mut SyncGraph,
    from_mutex: *const c_char,
    to_mutex: *const c_char,
    context: *const c_char,
) -> bool {
    let Some(graph) = graph_mut(graph) else {
        return false;
    };
    let Some(from_mutex) = read_cstr(from_mutex) else {
        graph.set_error("sync analyzer received null from_mutex".to_owned());
        return false;
    };
    let Some(to_mutex) = read_cstr(to_mutex) else {
        graph.set_error("sync analyzer received null to_mutex".to_owned());
        return false;
    };
    let context = read_cstr(context).unwrap_or_else(|| "<unknown context>".to_owned());
    graph.add_lock_edge(&from_mutex, &to_mutex, &context)
}

#[unsafe(no_mangle)]
pub extern "C" fn aura_sync_graph_add_join_edge(
    graph: *mut SyncGraph,
    from_thread: *const c_char,
    to_thread: *const c_char,
    context: *const c_char,
) -> bool {
    let Some(graph) = graph_mut(graph) else {
        return false;
    };
    let Some(from_thread) = read_cstr(from_thread) else {
        graph.set_error("sync analyzer received null from_thread".to_owned());
        return false;
    };
    let Some(to_thread) = read_cstr(to_thread) else {
        graph.set_error("sync analyzer received null to_thread".to_owned());
        return false;
    };
    let context = read_cstr(context).unwrap_or_else(|| "<unknown context>".to_owned());
    graph.add_join_edge(&from_thread, &to_thread, &context)
}

#[unsafe(no_mangle)]
pub extern "C" fn aura_sync_graph_last_error(graph: *const SyncGraph) -> *const c_char {
    if graph.is_null() {
        return ptr::null();
    }

    let graph = unsafe { &*graph };
    graph.last_error.as_ptr()
}
